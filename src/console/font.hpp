// src/console/font.hpp
#pragma once

extern "C" {
    #include <Uefi.h>
    #include <stdint.h>
    #include <stddef.h>
}

namespace ribon::font {

    constexpr UINTN FONT_WIDTH  = 8;
    constexpr UINTN FONT_HEIGHT = 16;

    struct font_map_t {
        UINT32 fvm_code;   // Unicode range 시작점
        UINT16 fvm_base;   // font_bytes 내 글리프 시작 index
        UINT16 fvm_len;    // 이 코드포인트 범위의 길이
    };

    // ------------------------------
    // FreeBSD 폰트 데이터 (extern)
    // ------------------------------
    extern const UINT8      font_bytes[];
    extern const UINTN      font_bytes_count;
    extern const font_map_t font_mapping_normal[];
    extern const font_map_t font_mapping_bold[];
    const UINTN font_mapping_normal_count = 322;


    // 현재 커서 위치 (FrameBuffer 전용)
    void resetCursor();
    void setCursor(UINTN cx, UINTN cy);
    void advanceCursor();

    // Cursor Getter
    UINTN getCursorX();
    UINTN getCursorY();

    // 초기화 (glyph 데이터 로딩 등)
    void initFontRenderer();

    // 글자 출력
    void drawChar(CHAR16 codepoint);

    /** @brief Unicode -> FreeBSD glyph index 변환 */
    INTN findGlyphIndex(UINT32 cp);

    // printf 보조용 정수 포맷
    void UIntToDec(UINT64 v, CHAR16* out);
    void UIntToHex(UINT64 v, CHAR16* out);

} // namespace ribon::font