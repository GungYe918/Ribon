// src/gfx/framebuffer.cpp
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/Init.hpp>
#include <Ribon/Print.hpp>

namespace ribon::fb {

    static FrameBufferInfo gFB = {};

    bool initFrameBuffer() {
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
        return (gFB.base ? &gFB : nullptr);
    }

    // ---------------------------------------------------------
    // 베이스 픽셀 read/write
    // ---------------------------------------------------------

    bool inBounds(int x, int y) {
        if (!gFB.base)          return false;
        if (x < 0 || y < 0)     return false;
        if ((UINTN)x >= gFB.width)  return false;
        if ((UINTN)y >= gFB.height) return false;
        return true;
    }

    UINT32 readPixel(int x, int y) {
        UINTN idx = (UINTN)y * gFB.pixelsPerScanLine + (UINTN)x;
        return gFB.base[idx];
    }

    void writePixel(int x, int y, UINT32 rgba) {
        UINTN idx = (UINTN)y * gFB.pixelsPerScanLine + (UINTN)x;
        gFB.base[idx] = rgba;
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

    // 화면 전체 clear
    void clear(UINT8 r, UINT8 g, UINT8 b) {
        // UEFI GOP: 32bit pixel = 0xAABBGGRR (리틀엔디안 기준)
        UINT32 color = (255u << 24) | (b << 16) | (g << 8) | r;

        if (!gFB.base) return;  // gFB base 주소 없음

        UINTN totalPixels = gFB.pixelsPerScanLine * gFB.height;

        for (UINTN i = 0; i < totalPixels; i++) {
            gFB.base[i] = color;
        }
    }

    void debugDump(UINTN count) {
        ribon::IO::Print<ribon::IO::Tags::DEBUG>("FB Dump (first %d dwords):\n", count);

        for (UINTN i = 0; i < count; i++) {
            ribon::IO::Print<ribon::IO::Tags::DEBUG>(
                "[%04d] 0x%08x\n",
                i,
                gFB.base[i]
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
            gFB.width, gFB.height, gFB.pixelsPerScanLine,
            gFB.sizeInBytes,
            (UINTN)gFB.base
        );
    }

} // namespace ribon::fb