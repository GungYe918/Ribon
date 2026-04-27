// include/Ribon/Memory.hpp

#pragma once

#include <Ribon/EfiContext.hpp>
#include <Uefi.h>
#include <stddef.h>

// ------------------------------------------
// Internal “allocation tracing” state
// ------------------------------------------
static UINTN g_allocCount = 0;
static UINTN g_freeCount  = 0;
static UINTN g_allocBytes = 0;
static UINTN g_freeBytes  = 0;

namespace ribon::mem {

    inline EFI_STATUS AllocatePool(
        EFI_MEMORY_TYPE   type,
        UINTN             size,
        VOID            **buffer
    ) {
        auto bs = ribon::getBS();
        if (!bs) return EFI_UNSUPPORTED;

        return bs->AllocatePool(type, size, buffer);
    }

    inline EFI_STATUS FreePool(VOID* buffer) {
        auto bs = ribon::getBS();
        if (!bs) return EFI_UNSUPPORTED;

        return bs->FreePool(buffer);
    }

    inline EFI_STATUS AllocatePages(
        EFI_ALLOCATE_TYPE type,
        EFI_MEMORY_TYPE memoryType,
        UINTN pages,
        EFI_PHYSICAL_ADDRESS* memory
    ) {
        auto bs = ribon::getBS();
        if (!bs) return EFI_UNSUPPORTED;

        return bs->AllocatePages(type, memoryType, pages, memory);
    }

    inline EFI_STATUS FreePages(EFI_PHYSICAL_ADDRESS memory, UINTN pages) {
        auto bs = ribon::getBS();
        if (!bs) return EFI_UNSUPPORTED;

        return bs->FreePages(memory, pages);
    }

    inline EFI_STATUS AllocateZeroPool(
        EFI_MEMORY_TYPE type,
        UINTN           size,
        VOID          **buffer
    ) {
        EFI_STATUS status = AllocatePool(type, size, buffer);
        if (EFI_ERROR(status)) return status;

        auto bs = ribon::getBS();
        if (!bs) return EFI_UNSUPPORTED;

        bs->SetMem(*buffer, size, 0);
        return EFI_SUCCESS;
    }

    /**
     * @brief AllocateCopyPool — 메모리 복사 Allocate
     */
    inline EFI_STATUS AllocateCopyPool(
        EFI_MEMORY_TYPE type,
        UINTN           size,
        VOID*           src,
        VOID          **dst
    ) {
        EFI_STATUS st = AllocatePool(type, size, dst);
        if (EFI_ERROR(st)) return st;

        auto bs = ribon::getBS();
        bs->CopyMem(*dst, src, size);
        return EFI_SUCCESS;
    }

    /**
     * @brief ReallocatePool - C-style realloc()
     */
    inline EFI_STATUS ReallocatePool(
        EFI_MEMORY_TYPE type,
        UINTN           oldSize,
        UINTN           newSize,
        VOID          **buffer
    ) {
        VOID* old = *buffer;
        VOID* newBuf = nullptr;

        EFI_STATUS st = AllocatePool(type, newSize, &newBuf);
        if (EFI_ERROR(st)) return st;

        auto bs = ribon::getBS();
        bs->CopyMem(newBuf, old, oldSize < newSize ? oldSize : newSize);

        FreePool(old);
        *buffer = newBuf;

        return EFI_SUCCESS;
    }

    /**
     * @brief AllocateAlignedPool - 정렬된 메모리 할당
     */
    inline EFI_STATUS AllocateAlignedPool(
        EFI_MEMORY_TYPE type,
        UINTN           size,
        UINTN           alignment,
        VOID          **buffer
    ) {
        UINTN total = size + alignment;
        VOID* raw = nullptr;

        EFI_STATUS st = AllocatePool(type, total, &raw);
        if (EFI_ERROR(st)) return st;

        UINTN addr = (UINTN)raw;
        UINTN aligned = (addr + (alignment - 1)) & ~(alignment - 1);

        *buffer = (VOID*)aligned;
        return EFI_SUCCESS;
    }

    /**
     * @brief GetMemoryStats - UEFI Memory Map을 사용해서 전체/사용가능 메모리 조회
     */
    inline EFI_STATUS GetMemoryStats(UINTN* totalMemory, UINTN* freeMemory) {
        auto bs = ribon::getBS();
        if (!bs) return EFI_UNSUPPORTED;

        EFI_MEMORY_DESCRIPTOR* memMap = nullptr;
        UINTN memMapSize = 0, mapKey = 0, descSize = 0;
        UINT32 descVersion = 0;

        // 1) 크기 먼저 질의
        EFI_STATUS st = bs->GetMemoryMap(
            &memMapSize,
            memMap,
            &mapKey,
            &descSize,
            &descVersion
        );
        if (st != EFI_BUFFER_TOO_SMALL) {
            return st;
        }

        // 2) 그 크기만큼 버퍼 할당
        st = ribon::mem::AllocatePool(EfiLoaderData, memMapSize, (void**)&memMap);
        if (EFI_ERROR(st) || !memMap) {
            return EFI_OUT_OF_RESOURCES;
        }

        // 3) 실제 메모리맵 읽기
        st = bs->GetMemoryMap(
            &memMapSize,
            memMap,
            &mapKey,
            &descSize,
            &descVersion
        );
        if (EFI_ERROR(st)) {
            ribon::mem::FreePool(memMap);
            return st;
        }

        UINTN total = 0;
        UINTN freeM = 0;

        const UINTN entryCount = memMapSize / descSize;
        for (UINTN i = 0; i < entryCount; ++i) {
            EFI_MEMORY_DESCRIPTOR* d =
                (EFI_MEMORY_DESCRIPTOR*)((UINT8*)memMap + i * descSize);

            const UINTN bytes = d->NumberOfPages * 4096;

            total += bytes;
            if (d->Type == EfiConventionalMemory) {
                freeM += bytes;
            }
        }

        *totalMemory = total;
        *freeMemory  = freeM;

        ribon::mem::FreePool(memMap);
        return EFI_SUCCESS;
    }

    inline UINTN GetAvailableMemory() {
        UINTN total = 0, freeM = 0;
        if (EFI_ERROR(GetMemoryStats(&total, &freeM))) return 0;
        return freeM;
    }

    /**
     * @brief memcpy — 메모리 블록을 복사 (겹침 없음)
     */
    inline void Memcpy(void* dst, const void* src, UINTN size) {
        auto bs = ribon::getBS();
        if (!bs) return;
        bs->CopyMem(dst, (void*)src, size);
    }

    /**
     * @brief memmove — 메모리 블록을 복사 (겹침 포함 안전)
     * UEFI CopyMem은 memmove처럼 겹치는 영역도 안전하게 처리한다.
     */
    inline void Memmove(void* dst, const void* src, UINTN size) {
        auto bs = ribon::getBS();
        if (!bs) return;
        bs->CopyMem(dst, (void*)src, size);
    }

    /**
     * @brief memset — 메모리 영역을 특정 값으로 채움
     */
    inline void Memset(void* dst, UINT8 value, UINTN size) {
        auto bs = ribon::getBS();
        if (!bs) return;
        bs->SetMem(dst, size, value);
    }


} // namespace ribon::mem



// ------------------------------------------------------------
// 전역 new / delete 연산자
// ------------------------------------------------------------

inline void* operator new(size_t size) {
    using namespace ribon::mem;

    void* buf = nullptr;

    // AllocatePool은 EFI_STATUS를 리턴하므로 래핑 필요
    EFI_STATUS st = AllocatePool(EfiLoaderData, size, &buf);
    if (EFI_ERROR(st) || !buf) {
        // UEFI에서 new 실패 → 반드시 nullptr 대신 예외 대신 abort 또는 nullptr
        return nullptr;
    }

    g_allocCount++;
    g_allocBytes += size;
    return buf;
}

inline void operator delete(void* ptr) noexcept {
    using namespace ribon::mem;

    if (!ptr) return;

    // 크기를 추적할 방법이 없으므로 freeBytes는 증가시키지 않음
    g_freeCount++;

    FreePool(ptr);
}

inline void* operator new[](size_t size) {
    using namespace ribon::mem;

    void* buf = nullptr;
    EFI_STATUS st = AllocatePool(EfiLoaderData, size, &buf);

    if (EFI_ERROR(st) || !buf)
        return nullptr;

    g_allocCount++;
    g_allocBytes += size;
    return buf;
}

inline void operator delete[](void* ptr) noexcept {
    using namespace ribon::mem;

    if (!ptr) return;

    g_freeCount++;
    FreePool(ptr);
}


// ---------------------------------------
// Sized delete (for C++14~17 ABI 호환)
// ---------------------------------------
inline void operator delete(void* ptr, size_t) noexcept {
    operator delete(ptr);
}

inline void operator delete[](void* ptr, size_t) noexcept {
    operator delete[](ptr);
}
