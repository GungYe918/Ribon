#include <Ribon/base/Utf16String.hpp>
#include <Ribon/Memory.hpp>
#include <Uefi.h>

namespace ribon::str {
    
    void Utf16String::allocate(UINTN size) {
        buf = nullptr;
        // size = CHAR16 개수 (널 포함)
        len = size - 1;

        VOID* out = nullptr;

        // UTF-16은 EfiLoaderData 영역을 가장 일반적으로 사용
        if (EFI_ERROR(mem::AllocateZeroPool(EfiLoaderData, size * sizeof(CHAR16), &out))) {
            buf = nullptr;
            len = 0;
            return;
        }

        buf = (CHAR16*)out;
    }
    
    /** @brief ASCII -> UTF16 생성자 */
    Utf16String::Utf16String(const char* ascii) : buf(nullptr), len(0) {
        if (!ascii) {
            allocate(1);
            if (buf) buf[0] = 0;
            return;
        }

        // ASCII 길이
        UINTN ascii_len = 0;
        while (ascii[ascii_len] != '\0') ascii_len++;

        // UTF-16 버퍼 할당
        allocate(ascii_len + 1);
        if (!buf) return;

        // 변환
        for (UINTN i = 0; i < ascii_len; i++)
            buf[i] = (CHAR16)ascii[i];

        buf[ascii_len] = 0;
    }

    /** @brief UTF-16 -> UTF-16 복사 생성자 */
    Utf16String::Utf16String(const CHAR16* utf16) : buf(nullptr), len(0) {
        if (!utf16) {
            allocate(1);
            if (buf) buf[0] = 0;
            return;
        }

        UINTN utf_len = 0;
        while (utf16[utf_len] != 0) utf_len++;

        allocate(utf_len + 1);
        if (!buf) return;

        for (UINTN i = 0; i < utf_len; i++)
            buf[i] = utf16[i];

        buf[utf_len] = 0;
    }

    /** @brief 이동 생성자 */
    Utf16String::Utf16String(Utf16String&& other) noexcept {
        buf = other.buf;
        len = other.len;

        other.buf = nullptr;
        other.len = 0;
    }

    /** @brief 이동 대입 연산자 */
    Utf16String& Utf16String::operator=(Utf16String&& other) noexcept {
        if (this != &other) {

            if (buf)
                mem::FreePool(buf);

            buf = other.buf;
            len = other.len;

            other.buf = nullptr;
            other.len = 0;
        }

        return *this;
    }

    void Utf16String::push_back(CHAR16 c) {
        // 새 길이 = 기존 + 1 문자 + 널(1)
        UINTN new_len = len + 1;
        UINTN new_size_bytes = (new_len + 1) * sizeof(CHAR16);

        VOID* tmp = nullptr;

        // 새 버퍼 할당
        if (EFI_ERROR(mem::AllocatePool(EfiBootServicesData, new_size_bytes, &tmp)))
            return;

        CHAR16* newbuf = (CHAR16*)tmp;

        // 기존내용 복사
        for (UINTN i = 0; i < len; i++)
            newbuf[i] = buf[i];

        // 새로운 문자 추가
        newbuf[len] = c;
        newbuf[len + 1] = 0;

        // 기존 버퍼 해제
        if (buf)
            mem::FreePool(buf);

        // 교체
        buf = newbuf;
        len = new_len;
    }

    void Utf16String::copy_from_utf16(const Utf16String& other) {
        if (this == &other) return;

        UINTN other_len = other.len;

        // 새 버퍼 할당
        UINTN new_size_bytes = (other_len + 1) * sizeof(CHAR16);
        VOID* tmp = nullptr;

        if (EFI_ERROR(mem::AllocatePool(EfiBootServicesData, new_size_bytes, &tmp)))
            return;

        CHAR16* newbuf = (CHAR16*)tmp;

        // 문자열 복사
        for (UINTN i = 0; i < other_len; i++)
            newbuf[i] = other.buf[i];

        newbuf[other_len] = 0;

        // 기존 메모리 해제
        if (buf)
            mem::FreePool(buf);

        // 교체
        buf = newbuf;
        len = other_len;
    }

    // 소멸자
    Utf16String::~Utf16String() {
        if (buf) mem::FreePool(buf);
    }

    void Utf16String::append(const CHAR16* tail) {
        if (!tail) return;

        UINTN tail_len = 0;
        while (tail[tail_len]) tail_len++;

        UINTN new_size_bytes = (len + tail_len + 1) * sizeof(CHAR16);

        VOID* tmp = nullptr;

        // UEFI 표준 메모리 타입: EfiBootServicesData
        if (EFI_ERROR(mem::AllocatePool(EfiBootServicesData, new_size_bytes, &tmp)))
            return;

        CHAR16* newbuf = (CHAR16*)tmp;

        // 기존 문자열 복사
        for (UINTN i = 0; i < len; i++)
            newbuf[i] = buf[i];

        // 뒤에 tail 복사
        for (UINTN j = 0; j < tail_len; j++)
            newbuf[len + j] = tail[j];

        newbuf[len + tail_len] = 0;

        // 기존 buf 해제
        mem::FreePool(buf);

        // 교체
        buf = newbuf;
        len = len + tail_len;
    }



    void Utf16String::append(const char* utf8_tail) {
        Utf16String tmp(utf8_tail);
        append(tmp.c_str());
    }

    void Utf16String::append(const Utf16String& other) {
        append(other.c_str());
    }
}