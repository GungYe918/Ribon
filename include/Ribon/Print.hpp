#pragma once
#include <Uefi.h>
#include <stdarg.h>
#include <stddef.h>

#include <Ribon/base/Utf8String.hpp>
#include <Ribon/base/Utf16String.hpp>

namespace ribon::IO {

    void Print(const char* fmt, ...);
    void Utf16Print(const CHAR16* wstr);
    void PrintRaw(const char* str);

    namespace Tags {
        struct UTF8 {};
        struct UTF16 {};
        struct RAW {};
        struct CHAR {};
        struct DEBUG {};
    }

    template <typename A, typename B>
    struct is_same { static const bool value = false; };

    template <typename A>
    struct is_same<A, A> { static const bool value = true; };

    // 포맷팅 엔진 선언
    void FormatPrint(const char* utf8_fmt, va_list args);

} // namespace ribon::IO


// --------------------------------------------
// detail 네임스페이스 forward declaration
// --------------------------------------------
namespace ribon::IO::detail {

    // NormalizeAndOutput(CHAR16*)
    void NormalizeAndOutput(const CHAR16* src);

    // 내부 포맷 변환
    ribon::str::Utf16String FormatToUtf16(const char* fmt, va_list args);

} // namespace ribon::IO::detail



// ---------------------------------------------------------
// 템플릿 함수 구현 (헤더 유지 OK)
// ---------------------------------------------------------
namespace ribon::IO {

    template <typename Mode>
    void Print(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);

        if constexpr (is_same<Mode, Tags::RAW>::value) {
            ribon::str::Utf16String u16(fmt);
            Utf16Print(u16.c_str());
        }
        else if constexpr (is_same<Mode, Tags::DEBUG>::value) {
            ribon::str::Utf16String prefix("[DBG] ");
            auto formatted = detail::FormatToUtf16(fmt, args);
            prefix.append(formatted);
            Utf16Print(prefix.c_str());
        }
        else {
            FormatPrint(fmt, args);
        }

        va_end(args);
    }


    template <typename Mode>
    void Print(const CHAR16* wfmt, ...)
    {
        ribon::str::Utf8String utf8_fmt(wfmt);

        va_list args;
        va_start(args, wfmt);

        if constexpr (is_same<Mode, Tags::DEBUG>::value) {
            ribon::str::Utf16String prefix(L"[DBG] ");
            auto formatted = detail::FormatToUtf16(utf8_fmt.c_str(), args);
            prefix.append(formatted);
            Utf16Print(prefix.c_str());
        }
        else {
            FormatPrint(utf8_fmt.c_str(), args);
        }

        va_end(args);
    }


    template <typename Mode>
    void Print(char c)
    {
        CHAR16 tmp[2] = {(CHAR16)c, 0};
        Utf16Print(tmp);
    }

    template <typename Mode>
    void Print(const ribon::str::Utf16String& s)
    {
        Utf16Print(s.c_str());
    }

    template <typename Mode>
    void Print(const ribon::str::Utf8String& s)
    {
        ribon::str::Utf16String u16(s.c_str());
        Utf16Print(u16.c_str());
    }

} // namespace ribon::IO
