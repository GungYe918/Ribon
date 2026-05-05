#pragma once

extern "C" {
#include <Uefi.h>
}

#include <Ribon/boot/Types.hpp>

namespace ribon::arch {

[[nodiscard]] UINT16 CurrentElfMachine();
[[nodiscard]] UINT32 CurrentElfPageSize();
[[nodiscard]] ribon::boot::BootArch CurrentBootArch();

enum : UINT32 {
    kKernelEntryFlagEnteredHigh = (1u << 1),
    kKernelEntryFlagLowTrampoline = (1u << 2),
    kKernelEntryFlagRibonDirectHigh = (1u << 3),
};

struct KernelJumpRequest {
    UINT64 fallback_entry = 0;
    UINT64 high_entry = 0;
    UINT64 high_entry_load = 0;
    UINT64 high_vaddr_start = 0;
    UINT64 high_vaddr_end = 0;
    UINT64 high_load_start = 0;
    UINT64 high_load_end = 0;
    UINT64 arch_bootstrap0 = 0;
    UINT64 arch_bootstrap1 = 0;
    UINT64 arch_bootstrap2 = 0;
    UINT64 arch_bootstrap3 = 0;
    UINT32 entry_flags = 0;
    bool direct_high_enabled = false;
    bool direct_high_required = false;
    const void* arg = nullptr;
};

bool PrepareDirectHighEntry(const ribon::boot::LoadedKernelImage& image, KernelJumpRequest& request);
[[noreturn]] void JumpToKernel(const KernelJumpRequest& request);

} // namespace ribon::arch
