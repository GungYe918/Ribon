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

inline constexpr UINTN kMaxKernelImageSegments = 8;

struct LoadedKernelSegment {
    UINT64 vaddr = 0;
    UINT64 paddr = 0;
    UINT64 load_addr = 0;
    UINT64 runtime_addr = 0;
    UINT64 mem_size = 0;
    UINT64 file_size = 0;
    UINT64 align = 0;
    UINT64 flags = 0;
};

struct KernelProbeResult {
    KernelFormat format = KernelFormat::Unknown;
    HandoffKind preferred_handoff = HandoffKind::None;
    BootArch arch = BootArch::Unknown;
};

struct LoadedKernelImage {
    UINT64 entry = 0;
    UINT64 entry_vaddr = 0;
    UINT64 entry_load_addr = 0;
    UINT64 high_entry_vaddr = 0;
    UINT64 high_entry_load_addr = 0;
    UINT64 phys_start = 0;
    UINT64 phys_end = 0;
    UINT64 linked_vaddr_start = 0;
    UINT64 linked_vaddr_end = 0;
    UINT64 linked_paddr_start = 0;
    UINT64 linked_paddr_end = 0;
    UINT32 segment_count = 0;
    UINT32 direct_entry_flags = 0;
    KernelFormat format = KernelFormat::Unknown;
    BootArch arch = BootArch::Unknown;
    LoadAddressPolicy load_policy = LoadAddressPolicy::UsePaddrWhenAvailable;
    LoadedKernelSegment segments[kMaxKernelImageSegments]{};
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
    const char* dtb_path = "\\EFI\\KAIRON\\platform.dtb";
    bool ui_enabled = false;
    bool accept_dtb = true;
    bool require_dtb = false;
};

struct LoadedFileBuffer {
    void* data = nullptr;
    UINTN size = 0;
};

} // namespace ribon::boot
