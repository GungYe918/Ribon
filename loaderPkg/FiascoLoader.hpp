// loaderPkg/FiascoLoader.hpp
#pragma once

#include <Uefi.h>

#include "LoaderBase.hpp"

#include <Ribon/arch/KernelJump.hpp>
#include <Ribon/Elf.hpp>
#include <Ribon/boot/Types.hpp>
#include <Ribon/image/ElfImage.hpp>

namespace ribon::loaderPkg {

class FiascoLoader : public LoaderBase<FiascoLoader> {
public:
    constexpr FiascoLoader() = default;

    constexpr bool probe(const void* file, UINTN size) const {
        if (!file || size < sizeof(Elf64_Ehdr)) {
            return false;
        }

        const auto* eh = reinterpret_cast<const Elf64_Ehdr*>(file);
        if (!IS_ELF(*eh) || !IS_ELF64(*eh)) {
            return false;
        }

        const char* bytes = reinterpret_cast<const char*>(file);
        const UINTN scan_limit = (size < 65536u) ? size : 65536u;

        bool seen_fiasco = false;
        bool seen_l4re = false;
        for (UINTN i = 0; i + 6 < scan_limit; ++i) {
            if (!seen_fiasco &&
                bytes[i + 0] == 'F' &&
                bytes[i + 1] == 'i' &&
                bytes[i + 2] == 'a' &&
                bytes[i + 3] == 's' &&
                bytes[i + 4] == 'c' &&
                bytes[i + 5] == 'o') {
                seen_fiasco = true;
            }

            if (!seen_l4re &&
                bytes[i + 0] == 'L' &&
                bytes[i + 1] == '4' &&
                bytes[i + 2] == 'R' &&
                bytes[i + 3] == 'e') {
                seen_l4re = true;
            }
        }

        return seen_fiasco || seen_l4re;
    }

    bool loadImage(const void* file, UINTN size, ribon::boot::LoadedKernelImage& out) {
        const ribon::image::ElfLoadRequest request{
            ribon::arch::CurrentBootArch(),
            ribon::boot::KernelFormat::Fiasco,
            ribon::boot::LoadAddressPolicy::UseVaddr,
            ribon::arch::CurrentElfMachine(),
        };
        return ribon::image::LoadElfKernelImage(file, size, request, out);
    }

    ribon::boot::KernelProbeResult probeResult() const {
        return {
            ribon::boot::KernelFormat::Fiasco,
            ribon::boot::HandoffKind::None,
            ribon::arch::CurrentBootArch(),
        };
    }
};

} // namespace ribon::loaderPkg
