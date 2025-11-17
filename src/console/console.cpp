#include <Ribon/Console.hpp>
#include <Ribon/Init.hpp>
#include <Ribon/FrameBuffer.hpp>
#include <stdarg.h>
#include <stddef.h>

// 내부 Font 렌더링 시스템 (fb font)
#include "font.hpp"   // src 내부 전용, 공개 헤더 아님

namespace ribon::console {

    // 현재 콘솔 인스턴스
    static Console* s_console = nullptr;

    Console* getConsole() {
        return s_console;
    }

    void setConsole(Console* c) {
        s_console = c;
    }


    // =====================================================
    // Console 구현
    // =====================================================

    Console::Console() {
        currentMode = TextMode::SimpleText;
    }

    void Console::SetMode(TextMode mode) {
        currentMode = mode;

        if (mode == TextMode::SimpleText) {
            auto st = ribon::getST();
            if (st && st->ConOut)
                st->ConOut->ClearScreen(st->ConOut);
        }
        else if (mode == TextMode::FBFont) {
            // FBFont 들어가면 화면을 FrameBuffer 기준으로 지움
            ribon::fb::clear(0, 0, 0);

            // 폰트 렌더링 엔진 초기화
            ribon::font::initFontRenderer();
        }
    }


    void Console::write(const ribon::str::Utf16String& str) {
        const CHAR16* wstr = str.c_str();
        if (!wstr) return;

        if (currentMode == TextMode::SimpleText) {
            writeSimpleText(wstr);
        }
        else {
            writeFramebufferText(wstr);
        }
    }


    void Console::write(const char* str) {
        if (!str) return;
        ribon::str::Utf16String u16(str);
        write(u16);
    }


    // ---------------------------------------------------------
    // printf 스타일 지원 (Console 단에서 최소 구현)
    // ---------------------------------------------------------
    void Console::writef(const char* fmt, ...) {
        if (!fmt) return;

        CHAR16 buffer[512];
        UINTN bi = 0;

        va_list args;
        va_start(args, fmt);

        for (UINTN i = 0; fmt[i] != '\0' && bi < 511; i++) {

            if (fmt[i] == '%' && fmt[i + 1] != 0) {
                i++;
                CHAR16 numbuf[64];

                if (fmt[i] == 'd' || fmt[i] == 'u') {
                    UINTN v = va_arg(args, UINTN);
                    ribon::font::UIntToDec(v, numbuf);
                }
                else if (fmt[i] == 'x') {
                    UINTN v = va_arg(args, UINTN);
                    ribon::font::UIntToHex(v, numbuf);
                }
                else if (fmt[i] == 's') {
                    const char* s = va_arg(args, const char*);
                    ribon::str::Utf16String u16(s);
                    const CHAR16* ws = u16.c_str();
                    for (UINTN j = 0; ws[j] != 0 && bi < 511; j++)
                        buffer[bi++] = ws[j];
                    continue;
                }
                else {
                    buffer[bi++] = L'%';
                    buffer[bi++] = fmt[i];
                    continue;
                }

                for (UINTN j = 0; numbuf[j] != 0 && bi < 511; j++)
                    buffer[bi++] = numbuf[j];
            }
            else {
                buffer[bi++] = (CHAR16)fmt[i];
            }
        }

        va_end(args);
        buffer[bi] = 0;

        ribon::str::Utf16String finalStr(buffer);
        write(finalStr);
    }


    void Console::setCursor(UINTN col, UINTN row) {
        if (currentMode != TextMode::SimpleText) return;
        auto st = ribon::getST();
        if (st && st->ConOut) st->ConOut->SetCursorPosition(st->ConOut, col, row);
    }


    void Console::clear() {
        if (currentMode == TextMode::SimpleText) {
            auto st = ribon::getST();
            if (st && st->ConOut) st->ConOut->ClearScreen(st->ConOut);
        } else {
            ribon::fb::clear(0,0,0);
            ribon::font::resetCursor();  // fb cursor reset
        }
    }


    // ------------------------------------------------------
    // 기존 UEFI ConOut 기반
    // ------------------------------------------------------
    void Console::writeSimpleText(const CHAR16* wstr) {
        auto st = ribon::getST();
        if (!st || !st->ConOut) return;

        st->ConOut->OutputString(st->ConOut, (CHAR16*)wstr);
    }


    // ------------------------------------------------------
    // FrameBuffer 폰트 기반 렌더링
    // ------------------------------------------------------
    void Console::writeFramebufferText(const CHAR16* wstr) {
        using namespace ribon::font;

        auto fb = ribon::fb::getFramebuffer();

        while (*wstr)
        {
            CHAR16 c = *wstr++;

            // ----- 제어 문자 처리 -----
            if (c == L'\n') {
                setCursor(0, getCursorY() + FONT_HEIGHT);
                continue;
            }
            if (c == L'\r') {
                setCursor(0, getCursorY());
                continue;
            }
            if (c == L'\t') {
                setCursor(getCursorX() + FONT_WIDTH * 4, getCursorY());
                continue;
            }

            // ----- 줄바꿈 처리 -----
            if (getCursorX() + FONT_WIDTH >= fb->width) {
                setCursor(0, getCursorY() + FONT_HEIGHT);
            }

            // ----- 정상 글자 출력 -----
            drawChar(c);
        }
    }


} // namespace ribon::console
