// src/gfx/framebuffer.cpp
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/Init.hpp>
#include <Ribon/Print.hpp>
#include <Ribon/Common.hpp>
#include <Ribon/Screen.hpp>
#include "doublebuffer.hpp"


namespace ribon::fb::detail {

    static FrameBufferInfo gFB = {};

    static inline UINT32* resolveTargetBase() {
        if (ribon::gfx::isDoubleBufferEnabled()) {
            auto bb = ribon::gfx::getDrawTarget();
            return bb ? bb->base : gFB.base;
        }
        return gFB.base;
    }

    static inline UINTN resolvePitch() {
        if (ribon::gfx::isDoubleBufferEnabled()) {
            auto bb = ribon::gfx::getDrawTarget();
            return bb ? bb->pixelsPerScan : gFB.pixelsPerScanLine;
        }
        return gFB.pixelsPerScanLine;
    }

} // namespace ribon::fb

namespace ribon::fb {

    bool initFrameBuffer() {
        using namespace detail;
        auto gop = ribon::getGop();
        if (!gop) {
            ribon::IO::Print<ribon::IO::Tags::DEBUG>(
                "GOP not initialized, framebuffer unavailable\n"
            );
            return false;
        }
        gFB.base = (UINT32*)gop->Mode->FrameBufferBase;
        gFB.width = gop->Mode->Info->HorizontalResolution;
        gFB.height = gop->Mode->Info->VerticalResolution;
        gFB.pixelsPerScanLine = gop->Mode->Info->PixelsPerScanLine;
        gFB.sizeInBytes = gop->Mode->FrameBufferSize;

        return true;
    }

    const FrameBufferInfo* getFramebuffer() {
        return (detail::gFB.base ? &detail::gFB : nullptr);
    }

    // ---------------------------------------------------------
    // 베이스 픽셀 read/write
    // ---------------------------------------------------------

    bool inBounds(int x, int y) {
        if (!detail::gFB.base)          return false;
        if (x < 0 || y < 0)     return false;
        if ((UINTN)x >= detail::gFB.width)  return false;
        if ((UINTN)y >= detail::gFB.height) return false;
        return true;
    }

    UINT32 readPixel(int x, int y) {
        UINT32* base = detail::resolveTargetBase();
        UINTN pitch  = detail::resolvePitch();
        return base[y * pitch + x];
    }

    void writePixel(int x, int y, UINT32 rgba) {
        UINT32* base = detail::resolveTargetBase();
        UINTN pitch  = detail::resolvePitch();
        base[y * pitch + x] = rgba;
    }

    void writePixelClamped(int x, int y, UINT32 rgba) {
        if (!inBounds(x, y)) return; // 경계 검사 실패
        writePixel(x, y, rgba);
    }

    // ---------------------------------------------------------
    // 과거 ABI 유지용 래퍼
    // ---------------------------------------------------------

    void writePixelRaw(UINTN x, UINTN y, UINT32 rgba) {
        writePixel((int)x, (int)y, rgba);
    }

    void writePixelSafe(UINTN x, UINTN y, UINT32 rgba) {
        writePixelClamped((int)x, (int)y, rgba);
    }

    void clear(UINT8 r, UINT8 g, UINT8 b) {
        UINT32* base = detail::resolveTargetBase();
        UINTN pitch  = detail::resolvePitch();

        // Screen 이 아직 초기화 되지 않았다면 RGBA 기본값으로 가정
        ribon::gfx::PixelFormat fmt = ribon::gfx::PixelFormat::RGBA;
        const auto& scr = ribon::gfx::getScreen();
        if (scr.width != 0 && scr.height != 0) {
            fmt = scr.format;
        }

        UINT32 color = ribon::gfx::makePixel(r, g, b, fmt);
        UINTN totalPixels = pitch * detail::gFB.height;

        for (UINTN i = 0; i < totalPixels; i++) {
            base[i] = color;
        }
    }


    void debugDump(UINTN count) {
        ribon::IO::Print<ribon::IO::Tags::DEBUG>("FB Dump (first %d dwords):\n", count);

        for (UINTN i = 0; i < count; i++) {
            ribon::IO::Print<ribon::IO::Tags::DEBUG>(
                "[%04d] 0x%08x\n",
                i,
                detail::gFB.base[i]
            );
        }
    }

    void debugPrintInfo() {
        ribon::IO::Print<ribon::IO::Tags::DEBUG>(
            "Framebuffer Info:\n"
            "  Resolution: %d x %d\n"
            "  PixelsPerScanLine: %d\n"
            "  SizeInBytes: %d\n"
            "  Base: 0x%lx\n",
            detail::gFB.width, detail::gFB.height, detail::gFB.pixelsPerScanLine,
            detail::gFB.sizeInBytes,
            (UINTN)detail::gFB.base
        );
    }

} // namespace ribon::fb