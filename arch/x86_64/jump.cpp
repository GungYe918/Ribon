#include <Ribon/arch/KernelJump.hpp>

namespace ribon::arch {

[[noreturn]] void JumpToKernel(UINT64 entry, const void* arg) {
    asm volatile(
        "mov %0, %%rdi\n"
        "jmp *%1\n"
        :
        : "r"(arg), "r"(entry)
        : "rdi");

    __builtin_unreachable();
}

} // namespace ribon::arch
