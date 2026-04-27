#pragma once

#include <Uefi.h>

#include "LoaderBase.hpp"
#include "Multiboot.hpp"

#include <Ribon/arch/KernelJump.hpp>
#include <Ribon/Elf.hpp>
#include <Ribon/boot/Types.hpp>
#include <Ribon/image/ElfImage.hpp>

namespace ribon::loaderPkg {

class MultibootLoader : public LoaderBase<MultibootLoader> {
public:
    constexpr MultibootLoader() = default;

    constexpr bool probe(const void* file, UINTN size) const {
        if (size < 32) {
            return false;
        }

        const UINT8* p = static_cast<const UINT8*>(file);

        for (UINTN i = 0; i + sizeof(MultibootHeader) <= size && i < 8192; i += 4) {
            auto hdr = reinterpret_cast<const MultibootHeader*>(p + i);
            if (hdr->magic == MULTIBOOT_HEADER_MAGIC) {
                UINT32 sum = hdr->magic + hdr->flags + hdr->checksum;
                if (sum == 0) {
                    return true;
                }
            }
        }

        for (UINTN i = 0; i + sizeof(Multiboot2Header) <= size && i < 8192; i += 8) {
            auto hdr = reinterpret_cast<const Multiboot2Header*>(p + i);
            if (hdr->magic == MULTIBOOT2_HEADER_MAGIC) {
                return true;
            }
        }

        return false;
    }

    bool loadImage(const void* file, UINTN size, ribon::boot::LoadedKernelImage& out) {
        const ribon::image::ElfLoadRequest request{
            ribon::arch::CurrentBootArch(),
            ribon::boot::KernelFormat::Multiboot,
            ribon::boot::LoadAddressPolicy::UseVaddr,
            ribon::arch::CurrentElfMachine(),
        };
        return ribon::image::LoadElfKernelImage(file, size, request, out);
    }

    ribon::boot::KernelProbeResult probeResult() const {
        return {
            ribon::boot::KernelFormat::Multiboot,
            ribon::boot::HandoffKind::None,
            ribon::arch::CurrentBootArch(),
        };
    }
};

} // namespace ribon::loaderPkg
