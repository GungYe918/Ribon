// src/console/font.cpp

#include "font.hpp"
#include <Ribon/Console.hpp>
#include <Ribon/FrameBuffer.hpp>


namespace ribon::font::detail {

    static inline UINT32 packRGBA(UINT8 r, UINT8 g, UINT8 b) {
        return (0xFFu << 24) | (UINT32(r) << 16) | (UINT32(g) << 8) | UINT32(b);
    }

} // namespace ribon::font::detail

namespace ribon::font {

    static UINTN cursorX = 0;
    static UINTN cursorY = 0;

    // 8x16 bitmap 글꼴 (추후 채우기)
    extern const unsigned char g_fontBitmap[256][16];

    void resetCursor() {
        cursorX = 0;
        cursorY = 0;
    }

    void setCursor(UINTN cx, UINTN cy) {
        cursorX = cx;
        cursorY = cy;
    }

    void advanceCursor() {
        cursorX += FONT_WIDTH;
        auto fb = ribon::fb::getFramebuffer();
        if (cursorX + FONT_WIDTH >= fb->width) {
            cursorX = 0;
            cursorY += FONT_HEIGHT;
        }
    }

    UINTN getCursorX() { return cursorX; }
    UINTN getCursorY() { return cursorY; }


    void initFontRenderer() {
        resetCursor();
    }

    INTN findGlyphIndex(UINT32 cp) {
        for (UINTN i = 0; i < font_mapping_normal_count; i++) {
            const auto& m = font_mapping_normal[i];

            UINT32 start = m.fvm_code;
            UINT32 end   = start + m.fvm_len;

            if (cp >= start && cp < end) {
                return (INTN)(m.fvm_base + (cp - start));
            }
        }

        return 0;  // fallback: 공백
    }


    void drawChar(CHAR16 c) {
        UINT32 cp = (UINT32)c;
        INTN glyphIndex = findGlyphIndex(cp);
        if (glyphIndex < 0) return;

        const UINT8* glyph = &font_bytes[glyphIndex * FONT_HEIGHT];

        auto fb = ribon::fb::getFramebuffer();
        if (!fb) return;

        // Console에서 현재 텍스트 색 가져오기
        UINT8 r,g,b,a;
        auto con = ribon::console::getConsole();
        if (con) con->getColor(r,g,b,a);
        else { r=g=b=255; a=255; }

        UINT32 color =
            (UINT32(a) << 24) |
            (UINT32(r) << 16) |
            (UINT32(g) << 8 ) |
            (UINT32(b));

        for (UINTN row = 0; row < FONT_HEIGHT; row++) {
            UINT8 bits = glyph[row];

            for (UINTN col = 0; col < FONT_WIDTH; col++) {
                UINT8 mask = 1 << (7 - col);  
                if (bits & mask) {
                    fb::writePixelSafe(
                        cursorX + col,
                        cursorY + row,
                        color
                    );
                }
            }
        }

        advanceCursor();
    }


    // 정수 포맷 변환
    void UIntToDec(UINT64 v, CHAR16* out) { /* 동일 로직 */ }
    void UIntToHex(UINT64 v, CHAR16* out) { /* 동일 로직 */ }

} // namespace ribon::font