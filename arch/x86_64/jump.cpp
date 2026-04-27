#include <Ribon/arch/KernelJump.hpp>
#include <Ribon/Elf.hpp>

namespace ribon::arch {

UINT16 CurrentElfMachine() {
    return EM_X86_64;
}

UINT32 CurrentElfPageSize() {
    return 4096;
}

ribon::boot::BootArch CurrentBootArch() {
    return ribon::boot::BootArch::X86_64;
}

[[noreturn]] void JumpToKernel(UINT64 entry, const void* arg) {
    asm volatile(
        "cld\n"
        "mov %0, %%rdi\n"
        "jmpq *%1\n"
        :
        : "r"(arg), "r"(entry)
        : "rdi", "memory");

    __builtin_unreachable();
}

} // namespace ribon::arch
