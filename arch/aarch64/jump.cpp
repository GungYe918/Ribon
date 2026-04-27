#include <Ribon/arch/KernelJump.hpp>
#include <Ribon/Elf.hpp>

namespace ribon::arch {

UINT16 CurrentElfMachine() {
    return EM_AARCH64;
}

UINT32 CurrentElfPageSize() {
    return 4096;
}

ribon::boot::BootArch CurrentBootArch() {
    return ribon::boot::BootArch::AArch64;
}

[[noreturn]] void JumpToKernel(UINT64, const void*) {
    for (;;) {
        asm volatile("wfe");
    }
}

} // namespace ribon::arch
