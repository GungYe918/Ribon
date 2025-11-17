// src/io/print.cpp
#include <Ribon/Print.hpp>
#include <Ribon/base/Utf8String.hpp>
#include <Ribon/base/Utf16String.hpp>
#include <Ribon/Init.hpp>
#include <Ribon/Console.hpp>


namespace ribon::IO::detail {

    // -------------------------------
    // Normalize: \n → \r\n 변환 등
    // Console의 writeSimpleText()가
    // 그대로 출력하므로 이 단계는 필요 없음
    // -------------------------------
    void NormalizeAndOutput(const CHAR16* src) 
    {
        auto con = ribon::console::getConsole();
        if (!con) return;

        // 그대로 Console에 전달
        ribon::str::Utf16String u16(src);
        con->write(u16);
    }


    // 정수 포맷 변환 ------------------------------------------

    static void UIntToDec(UINT64 v, CHAR16* out)
    {
        CHAR16 tmp[32];
        UINTN idx = 0;

        if (v == 0) {
            out[0] = L'0';
            out[1] = L'\0';
            return;
        }

        while (v > 0 && idx < 31) {
            tmp[idx++] = L'0' + (v % 10);
            v /= 10;
        }

        for (UINTN j = 0; j < idx; j++)
            out[j] = tmp[idx - j - 1];
        out[idx] = L'\0';
    }

    static void UIntToHex(UINT64 v, CHAR16* out)
    {
        static const CHAR16 HEX[] = {
            L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7',
            L'8', L'9', L'A', L'B', L'C', L'D', L'E', L'F',
            0
        };

        CHAR16 tmp[32];
        UINTN idx = 0;

        if (v == 0) {
            out[0] = L'0';
            out[1] = L'\0';
            return;
        }

        while (v > 0 && idx < 31) {
            tmp[idx++] = HEX[v & 0xF];
            v >>= 4;
        }

        for (UINTN j = 0; j < idx; j++)
            out[j] = tmp[idx - j - 1];

        out[idx] = L'\0';
    }


    // -------------------------------------------------------
    // FormatToUtf16: printf-style formatting → Utf16String
    // -------------------------------------------------------
    ribon::str::Utf16String FormatToUtf16(const char* fmt, va_list args)
    {
        CHAR16 buffer[512];
        UINTN bi = 0;

        for (UINTN i = 0; fmt[i] != '\0' && bi < 511; i++) {

            if (fmt[i] == '%' && fmt[i + 1] != '\0') {
                i++;

                CHAR16 numbuf[64];

                if (fmt[i] == 'd' || fmt[i] == 'u') {
                    UINTN v = va_arg(args, UINTN);
                    UIntToDec(v, numbuf);
                }
                else if (fmt[i] == 'x') {
                    UINTN v = va_arg(args, UINTN);
                    UIntToHex(v, numbuf);
                }
                else if (fmt[i] == 'l' && (fmt[i+1] == 'u' || fmt[i+1] == 'x')) {
                    UINT64 v = va_arg(args, UINT64);
                    if (fmt[i+1] == 'u') UIntToDec(v, numbuf);
                    else                UIntToHex(v, numbuf);
                    i++;
                }
                else if (fmt[i] == 's') {
                    const char* s = va_arg(args, const char*);
                    ribon::str::Utf16String u16(s);
                    const CHAR16* ws = u16.c_str();
                    for (UINTN j = 0; ws[j] != L'\0' && bi < 511; j++)
                        buffer[bi++] = ws[j];
                    continue;
                }
                else {
                    buffer[bi++] = L'%';
                    buffer[bi++] = fmt[i];
                    continue;
                }

                for (UINTN j = 0; numbuf[j] != L'\0' && bi < 511; j++)
                    buffer[bi++] = numbuf[j];
            }
            else {
                buffer[bi++] = (CHAR16)fmt[i];
            }
        }

        buffer[bi] = 0;
        return ribon::str::Utf16String(buffer);
    }

} // namespace ribon::IO::detail




// ===========================================================
//                  Public IO API (Console 기반)
// ===========================================================

namespace ribon::IO {

    // -------------------------------------------------------
    // FormatPrint → Console::write()
    // -------------------------------------------------------
    void FormatPrint(const char* utf8_fmt, va_list args)
    {
        auto con = ribon::console::getConsole();
        if (!con) return;  // console 미등록 시 출력 무시

        ribon::str::Utf16String s = detail::FormatToUtf16(utf8_fmt, args);
        con->write(s);
    }


    // -------------------------------------------------------
    // Utf16Print: Console::write() 직접 호출
    // -------------------------------------------------------
    void Utf16Print(const CHAR16* wstr)
    {
        auto con = ribon::console::getConsole();
        if (!con) return;

        ribon::str::Utf16String s(wstr);
        con->write(s);   // 끝!
    }


    // -------------------------------------------------------
    // PrintRaw: UTF8 → UTF16 변환 후 Console
    // -------------------------------------------------------
    void PrintRaw(const char* str)
    {
        if (!str) return;
        auto con = ribon::console::getConsole();
        if (!con) return;

        ribon::str::Utf16String utf16(str);
        con->write(utf16);
    }


    // -------------------------------------------------------
    // 기본 Print(fmt, ...)
    // -------------------------------------------------------
    void Print(const char* fmt, ...)
    {
        auto con = ribon::console::getConsole();
        if (!con) return;

        va_list args;
        va_start(args, fmt);

        ribon::str::Utf16String s = detail::FormatToUtf16(fmt, args);

        va_end(args);

        con->write(s);
    }

} // namespace ribon::IO