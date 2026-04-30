#pragma once

extern "C" {
#include <Uefi.h>
}

#include <Ribon/boot/Types.hpp>
#include <Ribon/platform/EfiPlatform.hpp>

namespace ribon::handoff {

struct LbpbBuildConfig {
    const char* boot_cmdline = nullptr;
    bool core_only = true;
    const void* device_tree = nullptr;
    UINTN device_tree_size = 0;
};

UINTN EstimateCoreLbpbSize(const ribon::platform::MemoryMapSnapshot& snapshot, const LbpbBuildConfig& config);

bool BuildCoreLbpb(
    const ribon::platform::BootContext& context,
    const ribon::platform::MemoryMapSnapshot& snapshot,
    const ribon::boot::LoadedKernelImage& image,
    const LbpbBuildConfig& config,
    void* buffer,
    UINTN capacity,
    ribon::boot::HandoffBlob& out
);

} // namespace ribon::handoff
