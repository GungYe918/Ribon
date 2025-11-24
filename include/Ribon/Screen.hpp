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
    static constexpr UINT32 makePixel(uint8_t r, uint8_t g, uint8_t b, PixelFormat fmt) {
        switch (fmt) {
        case PixelFormat::RGBA:
            // GOP: PixelRedGreenBlueReserved8BitPerColor
            // bits: [A][B][G][R]  -> 0xAABBGGRR
            return (0xFFu << 24) |
                (UINT32(b) << 16) |
                (UINT32(g) << 8)  |
                UINT32(r);

        case PixelFormat::BGRA:
        default:
            // GOP: PixelBlueGreenRedReserved8BitPerColor
            // bits: [A][R][G][B]  -> 0xAARRGGBB
            return (0xFFu << 24) |
                (UINT32(r) << 16) |
                (UINT32(g) << 8)  |
                UINT32(b);
        }
    }

    // FrameBuffer에 저장된 32bit 픽셀을 RGBA로 변환
    static constexpr void unpackPixel(
        UINT32 v, PixelFormat fmt,
        UINT8& r, UINT8& g, UINT8& b, UINT8& a
    ) {

        a = static_cast<uint8_t>((v >> 24) & 0xFFu);

        switch (fmt) {
        case PixelFormat::RGBA:
            // 0xAABBGGRR
            b = static_cast<UINT8>((v >> 16) & 0xFFu);
            g = static_cast<UINT8>((v >> 8)  & 0xFFu);
            r = static_cast<UINT8>( v        & 0xFFu);
            break;

        case PixelFormat::BGRA:
        default:
            // 0xAARRGGBB
            r = static_cast<UINT8>((v >> 16) & 0xFFu);
            g = static_cast<UINT8>((v >> 8)  & 0xFFu);
            b = static_cast<UINT8>( v        & 0xFFu);
            break;
        }
    }

} // namespace ribon::gfx