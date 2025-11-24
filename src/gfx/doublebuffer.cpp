#include "doublebuffer.hpp"

#include <Ribon/Common.hpp>
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/Memory.hpp>

extern "C" {
    #include <Uefi.h>
}

namespace ribon::gfx {

    static BackBufferInfo gBB {};
    static bool gEnabled = false;

    bool initDoubleBuffer() {
        auto& cfg = ribon::GetCommonConfig();
        if (cfg.renderMode != ribon::RenderMode::DoubleBuffer) {
            return false;
        }

        const auto fb = ribon::fb::getFramebuffer();
        if (!fb) return false;

        UINT64 total = fb->pixelsPerScanLine * fb->height;
        UINT64 bytes = total * sizeof(uint32_t);

        void* mem = nullptr;
        EFI_STATUS st = ribon::mem::AllocateZeroPool(EfiLoaderData, bytes, &mem);
        if (EFI_ERROR(st)) return false;
        
        gBB.base = (uint32_t*)mem;
        gBB.width = fb->width;
        gBB.height = fb->height;
        gBB.pixelsPerScan = fb->pixelsPerScanLine;

        gEnabled = true;
        return true;
    }

    void destroyDoubleBuffer() {
        if (gBB.base) {
            ribon::mem::FreePool(gBB.base);
            gBB.base = nullptr;
        }

        gEnabled = false;
    }

    bool isDoubleBufferEnabled() {
        return gEnabled;
    }


    const BackBufferInfo* getDrawTarget() {
        if (!gEnabled) return nullptr;
        return &gBB;
    }

    void present() {
        if (!gEnabled) return;

        const auto fb = ribon::fb::getFramebuffer();
        if (!fb) return;

        uint32_t* dst = fb->base;
        uint32_t* src = gBB.base;

        unsigned pitch = fb->pixelsPerScanLine;

        for (unsigned y = 0; y < fb->height; ++y) {
            uint32_t* drow = dst + y * pitch;
            uint32_t* srow = src + y * pitch;
            for (unsigned x = 0; x < pitch; ++x) {
                drow[x] = srow[x];
            }
        }
    }


} // namespace ribon::gfx
