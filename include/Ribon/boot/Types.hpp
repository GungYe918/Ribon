#pragma once

extern "C" {
#include <Uefi.h>
}

namespace ribon::boot {

enum class BootArch : UINT32 {
    Unknown = 0,
    X86_64,
    AArch64,
};

enum class KernelFormat : UINT32 {
    Unknown = 0,
    LBPB,
    Multiboot,
    Fiasco,
};

enum class HandoffKind : UINT32 {
    None = 0,
    LBPBCore,
};

enum class LoadAddressPolicy : UINT32 {
    UsePaddrWhenAvailable = 0,
    UseVaddr,
};

struct KernelProbeResult {
    KernelFormat format = KernelFormat::Unknown;
    HandoffKind preferred_handoff = HandoffKind::None;
    BootArch arch = BootArch::Unknown;
};

struct LoadedKernelImage {
    UINT64 entry = 0;
    UINT64 phys_start = 0;
    UINT64 phys_end = 0;
    KernelFormat format = KernelFormat::Unknown;
    BootArch arch = BootArch::Unknown;
    LoadAddressPolicy load_policy = LoadAddressPolicy::UsePaddrWhenAvailable;
};

struct HandoffBlob {
    void* data = nullptr;
    UINTN size = 0;
    HandoffKind kind = HandoffKind::None;
};

struct BootArtifact {
    LoadedKernelImage image{};
    HandoffBlob handoff{};
    UINT64 final_entry = 0;
};

struct BootPolicyConfig {
    const char* kernel_path = "\\kernel\\kernel.elf";
    const char* boot_cmdline = "kairon.autoboot=1";
    bool ui_enabled = false;
};

struct LoadedFileBuffer {
    void* data = nullptr;
    UINTN size = 0;
};

} // namespace ribon::boot
