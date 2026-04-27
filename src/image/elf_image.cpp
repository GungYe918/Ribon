#include <Ribon/image/ElfImage.hpp>

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

    UINT64 phys_min = ~UINT64(0);
    UINT64 phys_max = 0;

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

        UINT8* dst = reinterpret_cast<UINT8*>(static_cast<UINTN>(load_addr));
        const UINT8* src = base + p.p_offset;
        const UINTN filesz = static_cast<UINTN>(p.p_filesz);
        const UINTN memsz = static_cast<UINTN>(p.p_memsz);

        if (filesz > 0) {
            ribon::mem::Memcpy(dst, src, filesz);
        }
        if (memsz > filesz) {
            ribon::mem::Memset(dst + filesz, 0, memsz - filesz);
        }

        if (load_addr < phys_min) {
            phys_min = load_addr;
        }
        if (load_addr + p.p_memsz > phys_max) {
            phys_max = load_addr + p.p_memsz;
        }
    }

    if (phys_max <= phys_min) {
        return false;
    }

    out.entry = eh->e_entry;
    out.phys_start = phys_min;
    out.phys_end = phys_max;
    out.format = request.format;
    out.arch = request.arch;
    out.load_policy = request.load_policy;
    return true;
}

} // namespace ribon::image
