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

    void* load_buffer = nullptr;
    EFI_STATUS alloc_status = ribon::mem::AllocatePool(EfiLoaderCode, alloc_size, &load_buffer);
    if (EFI_ERROR(alloc_status) || !load_buffer) {
        return false;
    }

    const UINT64 runtime_base = reinterpret_cast<UINT64>(load_buffer);
    ribon::mem::Memset(load_buffer, 0, alloc_size);

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
