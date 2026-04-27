#include <Ribon/arch/KernelJump.hpp>

namespace ribon::arch {

[[noreturn]] void JumpToKernel(UINT64, const void*) {
    for (;;) {
        asm volatile("wfe");
    }
}

} // namespace ribon::arch
