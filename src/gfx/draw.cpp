// src/gfx/draw.cpp
#include <Ribon/Draw.hpp>
#include <Ribon/FrameBuffer.hpp>

namespace ribon::gfx::detail {

    /** @brief 픽셀 읽기 */
    static inline uint32_t readPixelUnsafe(int x, int y) {
        auto fb = ribon::fb::getFramebuffer();
        if (!fb) return 0;

        size_t index = y * fb->pixelsPerScanLine + x;
        return fb->base[index];
    }

    /** @brief 픽셀 쓰기 */
    static inline void writePixelUnsafe(int x, int y, uint32_t color) {
        auto fb = ribon::fb::getFramebuffer();
        if (!fb) return;

        size_t index = y * fb->pixelsPerScanLine + x;
        fb->base[index] = color;
    }

} // namespace ribon::gfx::detail

namespace ribon::gfx {

    /** @brief 알파 블렌딩 1픽셀 */
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

        // 완전 불투명: 그냥 덮어쓰기
        if (a == 255) {
            uint32_t color = (255u << 24) | (r << 16) | (g << 8) | b;
            detail::writePixelUnsafe(x, y, color);
            return;
        }

        // 완전 투명: 아무 것도 안 함
        if (a == 0) return;

        // 기존 픽셀 읽기
        uint32_t dst = detail::readPixelUnsafe(x, y);

        uint8_t dr = (dst >> 16) & 0xFF;
        uint8_t dg = (dst >> 8)  & 0xFF;
        uint8_t db =  dst        & 0xFF;

        // 알파 블렌딩
        float sa  = a / 255.0f;        // source alpha
        float inv = 1.0f - sa;         // 1 - alpha

        uint8_t rr = (uint8_t)(r * sa + dr * inv + 0.5f);
        uint8_t rg = (uint8_t)(g * sa + dg * inv + 0.5f);
        uint8_t rb = (uint8_t)(b * sa + db * inv + 0.5f);

        uint32_t out = (255u << 24) | (rr << 16) | (rg << 8) | rb;
        detail::writePixelUnsafe(x, y, out);
    }

    /** @brief 알파 블렌딩 사각형 */
    void drawRectAlpha(
        int x, int y, int w, int h,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a
    ) {
        auto fb = ribon::fb::getFramebuffer();
        if (!fb) return;

        int x1 = x + w;
        int y1 = y + h;

        if (x1 > (int)fb->width)  x1 = fb->width;
        if (y1 > (int)fb->height) y1 = fb->height;

        for (int yy = y; yy < y1; yy++) {
            for (int xx = x; xx < x1; xx++) {
                drawPixelAlpha(xx, yy, r, g, b, a);
            }
        }
    }

    /** @brief RGBA32 버퍼를 알파 블렌딩하여 화면에 렌더 */
    void drawImageRGBA(
        const uint32_t* px,
        int sx, int sy,
        int w, int h
    ) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                uint32_t c = px[y * w + x];

                uint8_t a = (c >> 24) & 0xFF;
                uint8_t r = (c >> 16) & 0xFF;
                uint8_t g = (c >> 8) & 0xFF;
                uint8_t b = c & 0xFF;

                drawPixelAlpha(sx + x, sy + y, r, g, b, a);
            }
        }
    }

} // namespace ribon::gfx