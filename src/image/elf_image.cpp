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

        if (load_addr < load_min) {
            load_min = load_addr;
        }
        if (load_addr + p.p_memsz > load_max) {
            load_max = load_addr + p.p_memsz;
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
     * AMD64 커널은 아직 relocation/MMU 단계가 없으므로 링커가 정한
     * 물리 주소에 그대로 올린다. AArch64는 기존처럼 page-aligned
     * 임의 loader 영역에 올려도 PC-relative 코드가 정상 동작한다.
     */
    EFI_ALLOCATE_TYPE alloc_type = AllocateAnyPages;
    EFI_PHYSICAL_ADDRESS phys_buffer = 0;
    if (request.arch == ribon::boot::BootArch::X86_64) {
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
    out.phys_start = runtime_base;
    out.phys_end = runtime_base + alloc_size;
    out.format = request.format;
    out.arch = request.arch;
    out.load_policy = request.load_policy;
    return true;
}

} // namespace ribon::image
