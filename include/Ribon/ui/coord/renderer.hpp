// include/Ribon/ui/renderer.hpp
#pragma once

#include "rect.hpp"
#include "transform.hpp"
#include <Ribon/Draw.hpp>
#include <Ribon/FrameBuffer.hpp>

namespace ribon::ui {

    /**
     * @brief Rect 영역을 알파 블렌딩을 통해 화면에 그린다.
     *
     * @param rect 화면 좌표 기준 Rect
     * @param r 0~255
     * @param g 0~255
     * @param b 0~255
     * @param a 0~255 (알파)
     *
     * @details
     *  UI 렌더러는 FrameBuffer에 직접 접근하지 않고,
     *  ribon::gfx::drawRectAlpha() API를 사용한다.
     *  이를 통해 알파 블렌딩, PNG, 이미지 위젯 등이 모두 일관된 렌더링 모델을 갖게 된다.
     */
    inline void drawRect(
        const ribon::coord::Rect& rect,
        unsigned char r, unsigned char g, unsigned char b, 
        unsigned char a = 255
    ) {
        // 좌표계에서 클램프
        int x0 = rect.x;
        int y0 = rect.y;
        int x1 = rect.x + rect.width;
        int y1 = rect.y + rect.height;

        auto fb = ribon::fb::getFramebuffer();
        if (!fb) return;

        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > (int)fb->width)  x1 = fb->width;
        if (y1 > (int)fb->height) y1 = fb->height;

        const int w = x1 - x0;
        const int h = y1 - y0;
        if (w <= 0 || h <= 0)
            return;

        // 알파 블렌딩 기반 렌더링
        ribon::gfx::drawRectAlpha(x0, y0, w, h, r, g, b, a);
    }

} // namespace ribon::ui
