#pragma once
 
extern "C" {
    #include <Uefi.h>
}

#include <stdint.h>


namespace ribon::gfx {

    enum class PixelFormat {
        BGRA,
        RGBA
    };  

    struct ScreenInfo {
        UINTN width;
        UINTN height;
        UINTN pixelsPerScanLine;
        PixelFormat format;
    };

    // 현재 스크린 정보 가져오기
    const ScreenInfo& getScreen();

    /** @brief 원하는 해상도로 GOP 모드를 변경하고 FrameBuffer 초기화 */
    bool initScreen(UINTN desiredWidth, UINTN desiredHeight);

    /** @brief 화면 지우기 (알파값 지원) */
    void clear(UINTN r, UINTN g, UINTN b, UINTN a);

    /**
     * @brief 픽셀 쓰기(알파 블렌딩 기반)
     * @details ABI를 위해 함수명 유지. 내부는 drawPixelAlpha()로 통일됨.
     */
    void putPixel(UINTN x, UINTN y, UINT8 r, UINT8 g, UINT8 b);
    
    // RGB값을 실제 픽셀 포맷(RGBA/BGRA)에 맞게 변환
    static constexpr UINT32 makePixel(UINT8 r, UINT8 g, UINT8 b, PixelFormat fmt) {
        return (fmt == PixelFormat::BGRA)
            ? ((255u << 24) | (b << 16) | (g << 8) | r)
            : ((255u << 24) | (r << 16) | (g << 8) | b);
    }

    // FrameBuffer에 저장된 32bit 픽셀을 RGBA로 변환
    static constexpr void unpackPixel(
        UINT32 px, PixelFormat fmt,
        UINT8& r, UINT8& g, UINT8& b, UINT8& a
    ) {
        a = static_cast<UINT8>((px >> 24) & 0xFFu);

        if (fmt == PixelFormat::BGRA) {
            b = static_cast<UINT8>((px >> 16) & 0xFFu);
            g = static_cast<UINT8>((px >> 8)  & 0xFFu);
            r = static_cast<UINT8>(px & 0xFFu);
        } else {
            r = static_cast<UINT8>((px >> 16) & 0xFFu);
            g = static_cast<UINT8>((px >> 8)  & 0xFFu);
            b = static_cast<UINT8>(px & 0xFFu);
        }
    }

} // namespace ribon::gfx