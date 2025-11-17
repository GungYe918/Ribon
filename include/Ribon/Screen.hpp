#pragma once
 
extern "C" {
    #include <Uefi.h>
}

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

    // init: 원하는 해상도 요청
    // width == height == 0이면 fallback (800x600)
    bool initScreen(UINTN desiredWidth, UINTN desiredHeight);

    // 화면 전체 지우기
    void clear(UINTN r, UINTN g, UINTN b, UINTN a);

    // RGB값을 실제 픽셀 포맷(RGBA/BGRA)에 맞게 변환
    inline UINT32 makePixel(UINT8 r, UINT8 g, UINT8 b, PixelFormat fmt)
    {
        if (fmt == PixelFormat::BGRA) {
            return (255 << 24) | (b << 16) | (g << 8) | r;
        } else {
            return (255 << 24) | (r << 16) | (g << 8) | b;
        }
    }

    // 빠른 inline clearPixel (프레임버퍼에 직접 접근)
    void putPixel(UINTN x, UINTN y, UINT8 r, UINT8 g, UINT8 b);

} // namespace ribon::gfx