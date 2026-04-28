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

[[noreturn]] void JumpToKernel(UINT64 entry, const void* arg) {
    register const void* x0 asm("x0") = arg;
    register UINT64 x16 asm("x16") = entry;
    asm volatile(
        "msr daifset, #0xf\n"
        "dsb sy\n"
        "ic ialluis\n"
        "dsb sy\n"
        "isb\n"
        "br x16\n"
        :
        : "r"(x0), "r"(x16)
        : "memory");

    __builtin_unreachable();
}

} // namespace ribon::arch
