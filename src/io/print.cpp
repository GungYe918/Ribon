// src/io/print.cpp
#include <Ribon/Print.hpp>
#include <Ribon/base/Utf8String.hpp>
#include <Ribon/base/Utf16String.hpp>
#include <Ribon/Init.hpp>
#include <Ribon/Console.hpp>


namespace ribon::IO::detail {

    // ===========================================================
    // 숫자 변환기
    // ===========================================================

    void NormalizeAndOutput(const CHAR16* src) 
    {
        auto con = ribon::console::getConsole();
        if (!con) return;

        // 그대로 Console에 전달
        ribon::str::Utf16String u16(src);
        con->write(u16);
    }


    // ===========================================================
    // 숫자 변환기
    // ===========================================================
        
    /** @brief 10진 변환 */
    void UIntToDec(uint64_t v, CHAR16* out)
    {
        CHAR16 tmp[32];
        uint32_t idx = 0;

        if (v == 0) {
            out[0] = L'0';
            out[1] = 0;
            return;
        }

        while (v > 0 && idx < 31) {
            tmp[idx++] = L'0' + (v % 10);
            v /= 10;
        }

        for (uint32_t j = 0; j < idx; j++)
            out[j] = tmp[idx - 1 - j];

        out[idx] = 0;
    }

    /** @brief 16진 변환 */
    void UIntToHex(uint64_t v, CHAR16* out) {
        static const CHAR16 HEX[] = {
            L'0',L'1',L'2',L'3',L'4',L'5',L'6',L'7',
            L'8',L'9',L'A',L'B',L'C',L'D',L'E',L'F'
        };

        CHAR16 tmp[32];
        uint32_t idx = 0;

        if (v == 0) {
            out[0] = L'0';
            out[1] = 0;
            return;
        }

        while (v > 0 && idx < 31) {
            tmp[idx++] = HEX[v & 0xF];
            v >>= 4;
        }

        for (uint32_t j = 0; j < idx; j++)
            out[j] = tmp[idx - 1 - j];

        out[idx] = 0;
    }

    // ===========================================================
    // 포맷 생성기 (출력 없음)
    // ===========================================================


    /**
     * @brief printf 스타일 -> UTF-16 변환
     */
    ribon::str::Utf16String FormatToUtf16(const char* fmt, va_list args) {
        CHAR16 buffer[512];
        uint32_t bi = 0;

        for (uint32_t i = 0; fmt[i] != '\0' && bi < 511; i++) {

            if (fmt[i] == '%' && fmt[i + 1]) {
                i++;

                CHAR16 numbuf[64];

                if (fmt[i] == 'd' || fmt[i] == 'u') {
                    uint64_t v = va_arg(args, uint64_t);
                    UIntToDec(v, numbuf);
                }
                else if (fmt[i] == 'x') {
                    uint64_t v = va_arg(args, uint64_t);
                    UIntToHex(v, numbuf);
                }
                else if (fmt[i] == 'l' && (fmt[i+1] == 'u' || fmt[i+1]=='x')) {
                    uint64_t v = va_arg(args, uint64_t);
                    if (fmt[i+1]=='u') UIntToDec(v, numbuf);
                    else              UIntToHex(v, numbuf);
                    i++;
                }
                else if (fmt[i] == 's') {
                    const char* s = va_arg(args, const char*);
                    ribon::str::Utf16String u16(s);
                    const CHAR16* ws = u16.c_str();
                    for (uint32_t j = 0; ws[j] && bi < 511; j++)
                        buffer[bi++] = ws[j];
                    continue;
                }
                else {
                    buffer[bi++] = L'%';
                    buffer[bi++] = fmt[i];
                    continue;
                }

                for (uint32_t j = 0; numbuf[j] && bi < 511; j++)
                    buffer[bi++] = numbuf[j];
            }
            else {
                buffer[bi++] = (CHAR16)fmt[i];
            }
        }

        buffer[bi] = 0;
        return ribon::str::Utf16String(buffer);
    }

    // ==================================
    // freePrint용 처리기
    // ==================================

    void BuildRawUtf16(const char* in_utf8, ribon::str::Utf16String& out) {
        out = ribon::str::Utf16String(in_utf8);
    }

    /**
     * @brief FreePrintRawAt 내부 동작
     */
    void InternalFreePrint(int x, int y, const CHAR16* raw) {
        if (!raw) return;

        auto con = ribon::console::getConsole();
        if (!con) return;

        // Console이 모드에 따라 적절히 처리 (FBFont 모드에서는 픽셀 좌표 기반)
        con->writeAt(x, y, raw);
    }

} // namespace ribon::IO::detail




// ===========================================================
//                  Public IO API (Console 기반)
// ===========================================================

namespace ribon::IO {

    /**
     * @brief FormatPrint: 문자열 변환만 하고 출력하지 않음.
     */
    void FormatPrint(const char* utf8_fmt, va_list args) {
        // UTF-16 변환까지만 수행
        ribon::str::Utf16String s = detail::FormatToUtf16(utf8_fmt, args);

        // 실제 출력은 콘솔 엔진이 담당
        auto con = ribon::console::getConsole();
        if (con)
            con->write(s);
    }


    
    /**
     * @brief Utf16Print: raw utf16을 콘솔로 전달
     */
    void Utf16Print(const CHAR16* wstr) {
        auto con = ribon::console::getConsole();
        if (!con) return;
        con->write(ribon::str::Utf16String(wstr));
    }


   
    /**
     * @brief PrintRaw: UTF8 -> UTF16 단순 변환 후 콘솔
     */
    void PrintRaw(const char* str)
    {
        auto con = ribon::console::getConsole();
        if (!con) return;

        con->write(ribon::str::Utf16String(str));
    }

    /**
     * @brief 기본 Print(fmt): format -> 콘솔
     */
    void Print(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);

        ribon::str::Utf16String s = detail::FormatToUtf16(fmt, args);

        va_end(args);

        auto con = ribon::console::getConsole();
        if (con)
            con->write(s);
    }

    /**
     * @brief FreePrintRawAt: 내부 raw 좌표 출력
     */
    void FreePrintRawAt(int x, int y, const CHAR16* raw) {
        // detail로 위임 (후속 시스템에서 처리)
        detail::InternalFreePrint(x, y, raw);
    }

} // namespace ribon::IO