// src/gfx/screen.cpp
#include <Ribon/Screen.hpp>
#include <Ribon/Draw.hpp>
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/Init.hpp>
#include <Ribon/Print.hpp>

#include "doublebuffer.hpp"

namespace ribon::gfx::detail {

    /** @brief GOP에서 원하는 해상도 찾기 */
    static INT32 findMode(UINTN w, UINTN h) {
        auto gop = ribon::getGop();
        if (!gop) return -1;

        for (UINT32 i = 0; i < gop->Mode->MaxMode; i++) {
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

    /** @brief GOP PixelFormat -> 내부 PixelFormat 변환 */
    static constexpr PixelFormat detectPixelFormat(UINT32 pf) {
        switch (pf) {
        case PixelBlueGreenRedReserved8BitPerColor:
            return PixelFormat::BGRA;
        case PixelRedGreenBlueReserved8BitPerColor:
            return PixelFormat::RGBA;
        default:
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
        // a=255(완전 불투명)으로 사용
        ribon::gfx::drawPixelAlpha((int)x, (int)y, r, g, b, 255);
    }

    // 화면 전체 Clear
    void clear(UINTN r, UINTN g, UINTN b, UINTN a) {
        auto fbInfo = ribon::fb::getFramebuffer();
        if (!fbInfo) return;

        // 완전 불투명: 현재 "드로우 타겟"(싱글/더블)에 대해 한방에 클리어
        if (a == 255) {
            ribon::fb::clear(
                static_cast<UINT8>(r),
                static_cast<UINT8>(g),
                static_cast<UINT8>(b)
            );
            return;
        }

        // 반투명 배경: 픽셀 단위 알파 블렌딩 (역시 드로우 타겟은 fb 모듈이 결정)
        for (UINTN y = 0; y < fbInfo->height; ++y) {
            for (UINTN x = 0; x < fbInfo->width; ++x) {
                drawPixelAlpha(
                    static_cast<int>(x),
                    static_cast<int>(y),
                    static_cast<uint8_t>(r),
                    static_cast<uint8_t>(g),
                    static_cast<uint8_t>(b),
                    static_cast<uint8_t>(a)
                );
            }
        }
    }


    bool initScreen(UINTN desiredWidth, UINTN desiredHeight) {
        auto gop = ribon::getGop();
        if (!gop) return false;

        if (desiredWidth == 0 || desiredHeight == 0) {
            desiredWidth = 800;
            desiredHeight = 600;
        }

        // 먼저 기존 fb 초기화
        ribon::fb::initFrameBuffer();

        // 목표 해상도 찾기
        INT32 mode = detail::findMode(desiredWidth, desiredHeight);

        if (mode >= 0) {
            gop->SetMode(gop, mode);
            ribon::IO::Print<ribon::IO::Tags::DEBUG>(
                "Screen mode changed to %dx%d\n",
                desiredWidth, desiredHeight
            );
        } else {
            ribon::IO::Print<ribon::IO::Tags::DEBUG>(
                "Requested %dx%d not found. Using default mode.\n",
                desiredWidth, desiredHeight
            );
        }

        // 다시 FrameBuffer 초기화
        ribon::fb::initFrameBuffer();
        const auto fb = ribon::fb::getFramebuffer();

        gScreen.width             = fb->width;
        gScreen.height            = fb->height;
        gScreen.pixelsPerScanLine = fb->pixelsPerScanLine;
        gScreen.format = detail::detectPixelFormat(
            gop->Mode->Info->PixelFormat
        );

        ribon::IO::Print<ribon::IO::Tags::DEBUG>(
            "Screen initialized: %dx%d, fmt=%s\n",
            gScreen.width, gScreen.height,
            (gScreen.format == PixelFormat::BGRA ? "BGRA" : "RGBA")
        );

        // -----------------------------
        // 더블 버퍼 연결
        // -----------------------------
        // (동일 함수 재호출 대비해서 먼저 정리)
        destroyDoubleBuffer();
        initDoubleBuffer();   // 내부에서 CommonConfig.renderMode == DoubleBuffer 인지 체크

        return true;
    }

    
    const ScreenInfo& getScreen() {
        return gScreen;
    }
    
} // namespace ribon::gfx