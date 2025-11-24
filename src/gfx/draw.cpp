// src/gfx/draw.cpp
#include <Ribon/Draw.hpp>
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/Screen.hpp>

#include <Ribon/Common.hpp>
#include "doublebuffer.hpp"

namespace ribon::gfx {

    /** @brief 알파 블렌딩 픽셀 */
    void drawPixelAlpha(
        int x, int y,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a
    ) {
        auto fb = ribon::fb::getFramebuffer();
        if (!fb) return;

        if (x < 0 || y < 0 ||
            x >= (int)fb->width ||
            y >= (int)fb->height)
            return;

        const auto& scr = getScreen();
        const PixelFormat fmt = scr.format;

        // 완전 투명: 아무 것도 안 함
        if (a == 0) return;

        // 완전 불투명: 그냥 덮어쓰기
        if (a == 255) {
            const UINT32 color = makePixel(r, g, b, fmt);
            ribon::fb::writePixel(x, y, color);
            return;
        }

        // 기존 픽셀 읽기
        const uint32_t dst = ribon::fb::readPixel(x, y);

        uint8_t dr, dg, db, da;
        unpackPixel(dst, fmt, dr, dg, db, da);

        // 알파 블렌딩
        const float sa  = a / 255.0f;        // source alpha
        const float inv = 1.0f - sa;         // 1 - alpha

        const uint8_t rr = static_cast<uint8_t>(r * sa + dr * inv + 0.5f);
        const uint8_t rg = static_cast<uint8_t>(g * sa + dg * inv + 0.5f);
        const uint8_t rb = static_cast<uint8_t>(b * sa + db * inv + 0.5f);

        const UINT32 out = makePixel(rr, rg, rb, fmt);
        ribon::fb::writePixel(x, y, out);
    }


    /** @brief 알파 블렌딩 사각형 */
    void drawRectAlpha(
        int x, int y, int w, int h,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a
    ) {
        auto fb = ribon::fb::getFramebuffer();
        if (!fb) return;

        int x0 = x;
        int y0 = y;
        int x1 = x + w;
        int y1 = y + h;

        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > (int)fb->width)  x1 = fb->width;
        if (y1 > (int)fb->height) y1 = fb->height;

        if (x0 >= x1 || y0 >= y1) return;

        for (int yy = y0; yy < y1; ++yy) {
            for (int xx = x0; xx < x1; ++xx) {
                drawPixelAlpha(xx, yy, r, g, b, a);
            }
        }
    }


    /** @brief RGBA32 버퍼를 알파 블렌딩하여 화면에 렌더
     *
     * px 버퍼는 RGBA8888 (byte 순서 R,G,B,A)의 row-major 배열이라고 가정.
     */
    void drawImageRGBA(
        const uint32_t* px,
        int sx, int sy,
        int w, int h
    ) {
        auto fb = ribon::fb::getFramebuffer();
        if (!fb) return;

        const uint8_t* src = reinterpret_cast<const uint8_t*>(px);

        for (int y = 0; y < h; ++y) {
            int dy = sy + y;
            if (dy < 0 || dy >= (int)fb->height)
                continue;

            for (int x = 0; x < w; ++x) {
                int dx = sx + x;
                if (dx < 0 || dx >= (int)fb->width)
                    continue;

                const int idx = (y * w + x) * 4;
                const uint8_t r = src[idx + 0];
                const uint8_t g = src[idx + 1];
                const uint8_t b = src[idx + 2];
                const uint8_t a = src[idx + 3];

                drawPixelAlpha(dx, dy, r, g, b, a);
            }
        }
    }

} // namespace ribon::gfx