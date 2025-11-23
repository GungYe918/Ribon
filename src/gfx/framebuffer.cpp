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
    // RAW 픽셀 쓰기 (경계 검사 없음)
    // ---------------------------------------------------------
    inline void writePixelRaw(UINTN x, UINTN y, UINT32 rgba)
    {
        UINTN idx = y * gFB.pixelsPerScanLine + x;
        gFB.base[idx] = rgba;
    }


    // ---------------------------------------------------------
    // 안전한 픽셀 쓰기
    // ---------------------------------------------------------
    void writePixelSafe(UINTN x, UINTN y, UINT32 rgba)
    {
        if (x >= gFB.width || y >= gFB.height)
            return;
        writePixelRaw(x, y, rgba);
    }


    // ---------------------------------------------------------
    // 화면 전체 Clear (RGB)
    // ---------------------------------------------------------
    void clear(UINT8 r, UINT8 g, UINT8 b)
    {
        // UEFI GOP: 32bit pixel = 0xAABBGGRR
        // A는 대부분 무시됨(255 추천)
        UINT32 color = (255 << 24) | (b << 16) | (g << 8) | r;

        UINTN totalPixels = gFB.pixelsPerScanLine * gFB.height;

        for (UINTN i = 0; i < totalPixels; i++)
            gFB.base[i] = color;
    }


    // ---------------------------------------------------------
    // Framebuffer memory dump
    // ---------------------------------------------------------
    void debugDump(UINTN count)
    {
        ribon::IO::Print<ribon::IO::Tags::DEBUG>("FB Dump (first %d dwords):\n", count);

        for (UINTN i = 0; i < count; i++)
        {
            ribon::IO::Print<ribon::IO::Tags::DEBUG>(
                "[%04d] 0x%08x\n",
                i,
                gFB.base[i]
            );
        }
    }


    // ---------------------------------------------------------
    // Framebuffer info debug 출력
    // ---------------------------------------------------------
    void debugPrintInfo()
    {
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