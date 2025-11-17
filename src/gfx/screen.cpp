#include <Ribon/Screen.hpp>
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/Init.hpp>
#include <Ribon/Print.hpp>

namespace ribon::gfx::detail {

    static INT32 findMode(UINTN w, UINTN h) {
        auto gop = ribon::getGop();
        if (!gop) return -1;

        for (UINT32 i = 0; i < gop->Mode->MaxMode; i++)
        {
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info;
            UINTN size;

            if (gop->QueryMode(gop, i, &size, &info) != EFI_SUCCESS)
                continue;

            if (info->HorizontalResolution == w &&
                info->VerticalResolution == h)
                return i;
        }

        return -1;
    }

    static PixelFormat detectPixelFormat() {
        auto gop = ribon::getGop();
        if (!gop) return PixelFormat::BGRA;

        auto info = gop->Mode->Info;

        switch (info->PixelFormat) {
            case PixelBlueGreenRedReserved8BitPerColor:
                return PixelFormat::BGRA;
            case PixelRedGreenBlueReserved8BitPerColor:
                return PixelFormat::RGBA;
            default:
                // 팔레트나 bitmask는 아주 드물지만 일단 BGRA 취급
                return PixelFormat::BGRA;
        }
    }

} // namespace ribon::gfx::detail

namespace ribon::gfx {

    static ScreenInfo gScreen = {
        0, 0, 0,
        PixelFormat::BGRA
    };

    // Pixel 쓰기
    void putPixel(UINTN x, UINTN y, UINT8 r, UINT8 g, UINT8 b) {
        auto fb = ribon::fb::getFramebuffer();

        if (!fb) return;
        if (x >= fb->width || y >= fb->height) return;

        UINT32 col = makePixel(r, g, b, gScreen.format);

        UINTN idx = y * fb->pixelsPerScanLine + x;
        fb->base[idx] = col;
    }

    // 화면 전체 Clear
    void clear(UINTN r, UINTN g, UINTN b, UINTN a) {
        auto fb = ribon::fb::getFramebuffer();

        if (!fb) return;
        
        UINT32 col = makePixel(r, g, b, gScreen.format);

        UINTN total = fb->pixelsPerScanLine * fb->height;
        for (UINTN i = 0; i < total; i++)
            fb->base[i] = col;
    }

    bool initScreen(UINTN desiredWidth, UINTN desiredHeight) {
        auto gop = ribon::getGop();
        if (!gop) return false;

        if (desiredWidth == 0 || desiredHeight == 0) {
            desiredWidth = 800;
            desiredHeight = 600;
        }

        // fb 초기화 first
        ribon::fb::initFrameBuffer();

        // 원하는 해상도 모드 찾기 
        INT32 mode = detail::findMode(desiredWidth, desiredHeight);

        if (mode < 0) {
            ribon::IO::Print<ribon::IO::Tags::DEBUG>(
                "Requested %dx%d not found. Using default mode.\n",
                desiredWidth, desiredHeight
            );
        } else {
            gop->SetMode(gop, mode);
            ribon::IO::Print<ribon::IO::Tags::DEBUG>(
                "Screen mode changed to %dx%d\n",
                desiredWidth, desiredHeight
            );
        }

        // 프레임버퍼 정보 초기화
        ribon::fb::initFrameBuffer();
        const auto fb = ribon::fb::getFramebuffer();

        // 스크린 정보 구축
        gScreen.width            = fb->width;
        gScreen.height           = fb->height;
        gScreen.pixelsPerScanLine = fb->pixelsPerScanLine;
        gScreen.format           = detail::detectPixelFormat();

        ribon::IO::Print<ribon::IO::Tags::DEBUG>(
            "Screen initialized: %dx%d, fmt=%s\n",
            gScreen.width, gScreen.height,
            (gScreen.format == PixelFormat::BGRA ? "BGRA" : "RGBA")
        );

        return true;
    }

    const ScreenInfo& getScreen() {
        return gScreen;
    }
    
} // namespace ribon::gfx