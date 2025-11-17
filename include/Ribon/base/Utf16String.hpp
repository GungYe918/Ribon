#pragma once
#include <Uefi.h>

namespace ribon::str {

    class Utf16String {
    public:
        /** @brief ASCII -> UTF-16 변환 생성자 */
        Utf16String(const char* ascii);
        
        /** @brief UTF-16문자열 복사 생성자 */
        Utf16String(const CHAR16* utf16);

        /** @brief (복사 생성자 & 복사 대입) 금지 (불필요한 동적 복사 방지) */
        Utf16String(const Utf16String&) = delete;
        Utf16String operator=(const Utf16String&) = delete;

        /** @brief 이동 생성자 & 이동 대입 허용 */
        Utf16String(Utf16String&& other) noexcept;
        Utf16String& operator=(Utf16String&& other) noexcept;

        ~Utf16String();


        CHAR16 *c_str() { return buf; }  
        const CHAR16 *c_str() const {  return buf;  }
        UINTN length() const {  return len;  }

        void append(const CHAR16* tail);
        void append(const char* utf8_tail);
        void append(const Utf16String& other);
    
    private:
        CHAR16* buf;
        UINTN   len = 0;

        void allocate(UINTN size);
    };
};