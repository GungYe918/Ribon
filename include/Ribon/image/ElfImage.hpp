#pragma once

extern "C" {
#include <Uefi.h>
}

#include <Ribon/boot/Types.hpp>

namespace ribon::image {

struct ElfLoadRequest {
    ribon::boot::BootArch arch = ribon::boot::BootArch::Unknown;
    ribon::boot::KernelFormat format = ribon::boot::KernelFormat::Unknown;
    ribon::boot::LoadAddressPolicy load_policy = ribon::boot::LoadAddressPolicy::UsePaddrWhenAvailable;
    UINT16 expected_machine = 0;
};

bool LoadElfKernelImage(
    const void* file,
    UINTN size,
    const ElfLoadRequest& request,
    ribon::boot::LoadedKernelImage& out
);

} // namespace ribon::image
