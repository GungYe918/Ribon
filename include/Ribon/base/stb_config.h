// stb_config.h
#pragma once

#include <Ribon/Memory.hpp>

// ----------------------------------------
// STB 전용 Allocator 함수들 선언
// 전부 inline
// ----------------------------------------
namespace ribon::stb {

    /// STB용 malloc
    inline void* UefiMalloc(UINTN size) {
        VOID* buf = nullptr;
        if (EFI_ERROR(ribon::mem::AllocatePool(EfiLoaderData, size + sizeof(UINTN), &buf))) {
            return nullptr;
        }

        // [size | 실제 데이터 ...]
        auto raw = static_cast<UINT8*>(buf);
        *reinterpret_cast<UINTN*>(raw) = size;
        return raw + sizeof(UINTN);
    }

    inline void UefiFree(void* ptr) {
        if (!ptr) return;
        auto raw = static_cast<UINT8*>(ptr) - sizeof(UINTN);
        ribon::mem::FreePool(raw);
    }

    inline UINTN UefiAllocSize(void* ptr) {
        if (!ptr) return 0;
        auto raw = static_cast<UINT8*>(ptr) - sizeof(UINTN);
        return *reinterpret_cast<UINTN*>(raw);
    }

    inline void* UefiRealloc(void* ptr, UINTN newSize) {
        // 새 할당
        if (!ptr) {
            return UefiMalloc(newSize);
        }

        UINTN oldSize = UefiAllocSize(ptr);
        void* newPtr = UefiMalloc(newSize);
        if (!newPtr) return nullptr;

        UINTN copySize = (oldSize < newSize) ? oldSize : newSize;
        ribon::mem::Memmove(newPtr, ptr, copySize);

        UefiFree(ptr);
        return newPtr;
    }

    inline void StbiMemCopy(void* dst, const void* src, UINTN size) {
        ribon::mem::Memcpy(dst, src, size);
    }

    inline void StbiMemSet(void* dst, UINT8 value, UINTN size) {
        ribon::mem::Memset(dst, value, size);
    }

} // namespace ribon::stb


// =============================
//   STB 매크로 바인딩
// =============================

#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS

// malloc / realloc / free
#define STBI_MALLOC(sz)        ::ribon::stb::UefiMalloc((UINTN)(sz))
#define STBI_REALLOC(p,sz)     ::ribon::stb::UefiRealloc((p), (UINTN)(sz))
#define STBI_FREE(p)           ::ribon::stb::UefiFree((p))

// memcpy / memset
#define STBI_MEMCOPY(d,s,n)    ::ribon::stb::StbiMemCopy((d),(s),(UINTN)(n))
#define STBI_MEMSET(p,v,n)     ::ribon::stb::StbiMemSet((p),(UINT8)(v),(UINTN)(n))

// assert 비활성화
#define STBI_ASSERT(x)


// 필요 없는 포맷 끄기 (PNG/JPG만 쓸 거라 가정)
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR

// 구현부 활성화
#define STB_IMAGE_IMPLEMENTATION
