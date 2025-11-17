#include <Ribon/Print.hpp>
#include <Ribon/base/Utf8String.hpp>
#include <Ribon/base/Utf16String.hpp>
#include <Ribon/Init.hpp>
#include <Uefi.h>

namespace ribon::IO::detail {

    static void NormalizeAndOutput(const CHAR16* src) {
        if (!getST() || !getST()->ConOut) {
            return;
        }

        CHAR16 tmp[1024];
        UINTN j = 0;

        for (UINTN i = 0; src[i] != L'\0' && j < 1022; i++) {
            CHAR16 c = src[i];

            if (c == L'\n') {
                tmp[j++] = L'\r';
                tmp[j++] = L'\n';
            }
            else if (c == L'\t') {
                for (int t = 0; t < 4 && j < 1022; t++)
                    tmp[j++] = L' ';
            }
            else if (c == L'\b') {
                if (j > 0) j--;
            }
            else {
                tmp[j++] = c;
            }
        }

        tmp[j] = L'\0';
        getST()->ConOut->OutputString(getST()->ConOut, tmp);
    }

    
    
    /**
     * @details 숫자 포멧 변환 함수들
     */

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


    /**
     * @brief printf 스타일 포맷팅을 수행하여 UTF-16 문자열을 돌려줌
     */
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
                    if (fmt[i+1] == 'u')
                        UIntToDec(v, numbuf);
                    else
                        UIntToHex(v, numbuf);
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



// --------------------------------
//      공통 출력 함수
// --------------------------------

namespace ribon::IO {

    void FormatPrint(const char* utf8_fmt, va_list args) {
        auto s = detail::FormatToUtf16(utf8_fmt, args);
        Utf16Print(s.c_str());
    }


    void Utf16Print(const CHAR16* wstr) {
        if (!wstr) return;

        detail::NormalizeAndOutput(wstr);
    }

    void PrintRaw(const char* str) {
        if (!str) return;
        // UTF-8 -> UTF-16 변환
        ribon::str::Utf16String utf16(str);
        Utf16Print(utf16.c_str());
    }

    // 기본 UTF8 Print
    void Print(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        auto s = detail::FormatToUtf16(fmt, args);
        va_end(args);

        Utf16Print(s.c_str());
    }

} // namespace ribon::IO