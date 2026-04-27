#pragma once

extern "C" {
#include <Uefi.h>
}

namespace ribon::arch {

[[noreturn]] void JumpToKernel(UINT64 entry, const void* arg);

} // namespace ribon::arch
