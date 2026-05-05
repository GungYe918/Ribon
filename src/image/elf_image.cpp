#include <Ribon/image/ElfImage.hpp>

#include <Ribon/arch/KernelJump.hpp>
#include <Ribon/Elf.hpp>
#include <Ribon/Memory.hpp>

namespace ribon::image {

bool LoadElfKernelImage(
    const void* file,
    UINTN size,
    const ElfLoadRequest& request,
    ribon::boot::LoadedKernelImage& out
) {
    out = {};

    if (!file || size < sizeof(Elf64_Ehdr)) {
        return false;
    }

    const auto* eh = reinterpret_cast<const Elf64_Ehdr*>(file);
    if (!IS_ELF(*eh) || !IS_ELF64(*eh)) {
        return false;
    }

    if (request.expected_machine != 0 && eh->e_machine != request.expected_machine) {
        return false;
    }

    const UINT8* base = reinterpret_cast<const UINT8*>(file);
    if (eh->e_phoff == 0 || eh->e_phnum == 0) {
        return false;
    }

    const UINT64 ph_end =
        static_cast<UINT64>(eh->e_phoff) +
        static_cast<UINT64>(eh->e_phnum) * sizeof(Elf64_Phdr);
    if (ph_end > size) {
        return false;
    }

    const auto* ph = reinterpret_cast<const Elf64_Phdr*>(base + eh->e_phoff);

    const UINT64 page_size = ribon::arch::CurrentElfPageSize();
    UINT64 load_min = ~UINT64(0);
    UINT64 load_max = 0;
    UINT64 vaddr_min = ~UINT64(0);
    UINT64 vaddr_max = 0;
    UINT64 paddr_min = ~UINT64(0);
    UINT64 paddr_max = 0;
    UINT64 high_vaddr_min = ~UINT64(0);
    UINT64 high_vaddr_max = 0;
    UINT64 high_load_min = ~UINT64(0);
    UINT64 actual_entry = 0;

    for (UINT16 i = 0; i < eh->e_phnum; ++i) {
        const Elf64_Phdr& p = ph[i];
        if (p.p_type != PT_LOAD || p.p_memsz == 0) {
            continue;
        }

        if (p.p_offset + p.p_filesz > size) {
            return false;
        }

        UINT64 load_addr = p.p_vaddr;
        if (request.load_policy == ribon::boot::LoadAddressPolicy::UsePaddrWhenAvailable && p.p_paddr != 0) {
            load_addr = p.p_paddr;
        }

        if (out.segment_count < ribon::boot::kMaxKernelImageSegments) {
            ribon::boot::LoadedKernelSegment& segment = out.segments[out.segment_count++];
            segment.vaddr = p.p_vaddr;
            segment.paddr = p.p_paddr;
            segment.load_addr = load_addr;
            segment.mem_size = p.p_memsz;
            segment.file_size = p.p_filesz;
            segment.align = p.p_align;
            segment.flags = p.p_flags;
        }

        if (load_addr < load_min) {
            load_min = load_addr;
        }
        if (load_addr + p.p_memsz > load_max) {
            load_max = load_addr + p.p_memsz;
        }
        if (p.p_vaddr < vaddr_min) {
            vaddr_min = p.p_vaddr;
        }
        if (p.p_vaddr + p.p_memsz > vaddr_max) {
            vaddr_max = p.p_vaddr + p.p_memsz;
        }
        if (p.p_paddr < paddr_min) {
            paddr_min = p.p_paddr;
        }
        if (p.p_paddr + p.p_memsz > paddr_max) {
            paddr_max = p.p_paddr + p.p_memsz;
        }
        if ((p.p_vaddr & 0xFFFF000000000000ull) == 0xFFFF000000000000ull) {
            if (p.p_vaddr < high_vaddr_min) {
                high_vaddr_min = p.p_vaddr;
                high_load_min = load_addr;
            }
            if (p.p_vaddr + p.p_memsz > high_vaddr_max) {
                high_vaddr_max = p.p_vaddr + p.p_memsz;
            }
        }

        if (eh->e_entry >= p.p_vaddr && eh->e_entry < p.p_vaddr + p.p_memsz) {
            actual_entry = load_addr + (eh->e_entry - p.p_vaddr);
        }
    }

    if (load_max <= load_min) {
        return false;
    }

    const UINT64 page_mask = page_size - 1;
    const UINT64 alloc_base = load_min & ~page_mask;
    const UINT64 alloc_end = (load_max + page_mask) & ~page_mask;
    const UINTN alloc_size = static_cast<UINTN>(alloc_end - alloc_base);

    /*
     * AArch64 (and many ELF kernels in general) generate ADRP+ADD/LDR
     * sequences for every global reference. ADRP encodes a PC-relative
     * page offset and assumes the binary is loaded at a page-aligned
     * base; AllocatePool only guarantees 8-byte alignment, which would
     * shift every symbol by `base & 0xFFF` at runtime. AllocatePages
     * gives us a 4 KiB-aligned buffer and keeps PIC references valid.
     */
    const UINTN page_bytes = static_cast<UINTN>(page_size);
    const UINTN alloc_pages =
        (alloc_size + page_bytes - 1) / page_bytes;
    /*
     * High-VMA dual-entry kernel은 low trampoline LMA를 기준으로 page
     * table을 만든다. p_paddr가 있는 LBPB kernel은 architecture와
     * 무관하게 ELF가 선언한 낮은 load window에 올려야 함.
     */
    EFI_ALLOCATE_TYPE alloc_type = AllocateAnyPages;
    EFI_PHYSICAL_ADDRESS phys_buffer = 0;
    if (request.load_policy == ribon::boot::LoadAddressPolicy::UsePaddrWhenAvailable &&
        paddr_min != ~UINT64(0)) {
        alloc_type = AllocateAddress;
        phys_buffer = alloc_base;
    }
    EFI_STATUS alloc_status = ribon::mem::AllocatePages(
        alloc_type,
        EfiLoaderCode,
        alloc_pages,
        &phys_buffer);
    if (EFI_ERROR(alloc_status) || phys_buffer == 0) {
        return false;
    }
    void* load_buffer = reinterpret_cast<void*>(static_cast<UINTN>(phys_buffer));

    const UINT64 runtime_base = reinterpret_cast<UINT64>(load_buffer);
    const UINTN backing_bytes = alloc_pages * page_bytes;
    ribon::mem::Memset(load_buffer, 0, backing_bytes);

    for (UINT16 i = 0; i < eh->e_phnum; ++i) {
        const Elf64_Phdr& p = ph[i];
        if (p.p_type != PT_LOAD || p.p_memsz == 0) {
            continue;
        }

        UINT64 load_addr = p.p_vaddr;
        if (request.load_policy == ribon::boot::LoadAddressPolicy::UsePaddrWhenAvailable && p.p_paddr != 0) {
            load_addr = p.p_paddr;
        }

        UINT8* dst = reinterpret_cast<UINT8*>(
            static_cast<UINTN>(runtime_base + (load_addr - alloc_base))
        );
        for (UINT32 segment_index = 0; segment_index < out.segment_count; ++segment_index) {
            if (out.segments[segment_index].load_addr == load_addr &&
                out.segments[segment_index].mem_size == p.p_memsz) {
                out.segments[segment_index].runtime_addr =
                    runtime_base + (load_addr - alloc_base);
                break;
            }
        }
        const UINT8* src = base + p.p_offset;
        const UINTN filesz = static_cast<UINTN>(p.p_filesz);
        const UINTN memsz = static_cast<UINTN>(p.p_memsz);

        if (filesz > 0) {
            ribon::mem::Memcpy(dst, src, filesz);
        }
        if (memsz > filesz) {
            ribon::mem::Memset(dst + filesz, 0, memsz - filesz);
        }

    }

    const UINT64 resolved_entry = actual_entry != 0 ? actual_entry : eh->e_entry;
    out.entry = runtime_base + (resolved_entry - alloc_base);
    out.entry_vaddr = eh->e_entry;
    out.entry_load_addr = resolved_entry;
    out.high_entry_vaddr = high_vaddr_min == ~UINT64(0) ? 0 : high_vaddr_min;
    out.high_entry_load_addr = high_load_min == ~UINT64(0) ? 0 : high_load_min;
    out.phys_start = runtime_base;
    out.phys_end = runtime_base + alloc_size;
    out.linked_vaddr_start = vaddr_min;
    out.linked_vaddr_end = vaddr_max;
    out.linked_paddr_start = paddr_min == ~UINT64(0) ? 0 : paddr_min;
    out.linked_paddr_end = paddr_max;
    out.format = request.format;
    out.arch = request.arch;
    out.load_policy = request.load_policy;
    return true;
}

} // namespace ribon::image
