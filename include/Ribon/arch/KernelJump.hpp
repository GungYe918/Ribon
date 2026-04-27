#pragma once

extern "C" {
#include <Uefi.h>
}

#include <Ribon/boot/Types.hpp>

namespace ribon::arch {

[[nodiscard]] UINT16 CurrentElfMachine();
[[nodiscard]] UINT32 CurrentElfPageSize();
[[nodiscard]] ribon::boot::BootArch CurrentBootArch();
[[noreturn]] void JumpToKernel(UINT64 entry, const void* arg);

} // namespace ribon::arch
