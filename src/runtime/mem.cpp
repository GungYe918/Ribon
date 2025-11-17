#include <Ribon/Memory.hpp>
#include <stddef.h>

extern "C" void* memcpy(void* dst, const void* src, size_t size) {
    ribon::mem::Memcpy(dst, src, size);
    return dst;
}

extern "C" void* memset(void* ptr, int value, size_t size) {
    ribon::mem::Memset(ptr, (UINT8)value, size);
    return ptr;
}
