// Ribon - LBPBLoader.hpp
#pragma once

#include "LoaderBase.hpp"

#include <Ribon/arch/KernelJump.hpp>
#include <Ribon/Elf.hpp>
#include <Ribon/boot/Types.hpp>
#include <Ribon/image/ElfImage.hpp>

namespace ribon::loaderPkg {

class LBPBLoader : public LoaderBase<LBPBLoader> {
public:
    constexpr LBPBLoader() = default;

    constexpr bool probe(const void* file, UINTN size) const {
        if (!file || size < sizeof(Elf64_Ehdr)) {
            return false;
        }

        const auto* eh = reinterpret_cast<const Elf64_Ehdr*>(file);
        return IS_ELF(*eh) && IS_ELF64(*eh) && eh->e_machine == ribon::arch::CurrentElfMachine();
    }

    bool loadImage(const void* file, UINTN size, ribon::boot::LoadedKernelImage& out) {
        const ribon::image::ElfLoadRequest request{
            ribon::arch::CurrentBootArch(),
            ribon::boot::KernelFormat::LBPB,
            ribon::boot::LoadAddressPolicy::UsePaddrWhenAvailable,
            ribon::arch::CurrentElfMachine(),
        };
        return ribon::image::LoadElfKernelImage(file, size, request, out);
    }

    ribon::boot::KernelProbeResult probeResult() const {
        return {
            ribon::boot::KernelFormat::LBPB,
            ribon::boot::HandoffKind::LBPBCore,
            ribon::arch::CurrentBootArch(),
        };
    }
};

} // namespace ribon::loaderPkg
