#pragma once

#include <Ribon/base/Utf8String.hpp>
#include <Ribon/base/Utf16String.hpp>

extern "C" {
    #include <Uefi.h>
    #include <stdarg.h>
    #include <stddef.h>
    #include <stdint.h>
}

namespace ribon::IO {

/**
 * @brief 기본 Print API
 *
 * 출력 엔진은 문자열_FORMATTING까지 수행하고,
 * 실제 출력(콘솔/위젯/기타)은 외부에서 수행한다.
 */
void Print(const char* fmt, ...);
void Utf16Print(const CHAR16* wstr);
void PrintRaw(const char* str);

/**
 * @brief 출력 모드 Tags
 */
namespace Tags {
    struct UTF8 {};
    struct UTF16 {};
    struct RAW {};
    struct CHAR {};
    struct DEBUG {};
    struct FREE_RAW {};     ///< raw 출력 + 좌표 기반 freePrint 모드
}

/** @brief 템플릿 타입 비교 */
template <typename A, typename B>
struct is_same { static const bool value = false; };

template <typename A>
struct is_same<A, A> { static const bool value = true; };

/**
 * @brief 포맷팅 전용 엔진. 문자열 변환만 하고 출력은 하지 않음.
 */
void FormatPrint(const char* utf8_fmt, va_list args);

/**
 * @brief raw 문자열을 특정 (x,y)에 출력하는 내부 함수
 * Widget, 콘솔 등 어디든 이 결과를 받아 그리는 시스템에서 사용.
 */
void FreePrintRawAt(int x, int y, const CHAR16* raw);

} // namespace ribon::IO


// ===================================================================
// detail 네임스페이스 — 내부 구현부
// ===================================================================
namespace ribon::IO::detail {

/**
 * @brief FormatToUtf16
 *
 * printf 스타일의 UTF-8 포맷 문자열을 UTF-16 문자열로 변환한다.
 * 출력은 하지 않으며, 단순히 변환 결과만 반환한다.
 */
ribon::str::Utf16String FormatToUtf16(const char* fmt, va_list args);

/**
 * @brief 정수 변환기
 */
void UIntToDec(uint64_t v, CHAR16* out);
void UIntToHex(uint64_t v, CHAR16* out);

/**
 * @brief freePrint 내부 문자열 처리기
 * raw CHAR16 문자열을 반환 (다른 곳에서 출력 처리)
 */
void BuildRawUtf16(const char* in_utf8, ribon::str::Utf16String& out);

/**
 * @brief freePrint 호출용 편의 함수
 */
void InternalFreePrint(int x, int y, const CHAR16* raw);

} // namespace ribon::IO::detail



// ===================================================================
// 템플릿 함수 구현
// ===================================================================
namespace ribon::IO {

/**
 * @brief 템플릿 Print(fmt)
 */
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
    else if constexpr (is_same<Mode, Tags::FREE_RAW>::value) {
        // raw 출력용 전달 (출력은 외부 엔진)
        ribon::str::Utf16String u16;
        detail::BuildRawUtf16(fmt, u16);
        FreePrintRawAt(0, 0, u16.c_str()); // 좌표는 후속 시스템에서 override
    }
    else {
        // 일반 포맷팅
        FormatPrint(fmt, args);
    }

    va_end(args);
}

/**
 * @brief CHAR16 기반 템플릿 Print()
 */
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
    else if constexpr (is_same<Mode, Tags::FREE_RAW>::value) {
        ribon::str::Utf16String u16(wfmt);
        FreePrintRawAt(0, 0, u16.c_str());
    }
    else {
        FormatPrint(utf8_fmt.c_str(), args);
    }

    va_end(args);
}

/**
 * @brief 단일 문자 출력
 */
template <typename Mode>
void Print(char c)
{
    CHAR16 tmp[2] = { (CHAR16)c, 0 };
    Utf16Print(tmp);
}

/**
 * @brief Utf16String 출력
 */
template <typename Mode>
void Print(const ribon::str::Utf16String& s)
{
    Utf16Print(s.c_str());
}

/**
 * @brief Utf8String 출력
 */
template <typename Mode>
void Print(const ribon::str::Utf8String& s)
{
    ribon::str::Utf16String u16(s.c_str());
    Utf16Print(u16.c_str());
}

} // namespace ribon::IO
