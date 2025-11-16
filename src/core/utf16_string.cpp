#include <Ribon/Utf16String.hpp>
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

    // 소멸자
    Utf16String::~Utf16String() {
        if (buf) mem::FreePool(buf);
    }
}