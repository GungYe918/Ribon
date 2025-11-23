// Ribon/ui/coord/transform.hpp

#pragma once

#include "point.hpp"
#include "rect.hpp"
#include "absolute.hpp"
#include "relative.hpp"

#include <Ribon/Screen.hpp>   // 화면 크기 정보 사용

/**
 * @file Transform.hpp
 * @brief 절대/상대 좌표계를 실제 화면 좌표(Point)로 변환하는 함수들을 제공.
 *
 */

namespace ribon::coord::transform {

    /**
     * @brief AbsolutePos -> 실제 화면 좌표로 변환.
     * @param abs AbsolutePos 구조체
     * @return 계산된 Point (x, y)
     *
     * AbsolutePos는 이미 화면 기준 절대값이므로 단순 복사.
     */
    inline Point toPoint(const AbsolutePos& abs) {
        return Point{ abs.x, abs.y };
    }

    /**
     * @brief RelativePos -> 부모 Rect 기준으로 실제 화면 좌표 계산.
     * @param rel RelativePos 구조체
     * @return 실제 화면 Point
     *
     * 부모가 nullptr이면 화면 전체(Rect(0,0,screen.width,screen.height))를 기준으로 본다.
     */
    inline Point toPoint(const RelativePos& rel) {
        int parentX = 0;
        int parentY = 0;
        int parentW = 0;
        int parentH = 0;

        if (rel.parent) {
            const Rect& p = *rel.parent;
            parentX = p.x;
            parentY = p.y;
            parentW = p.width;
            parentH = p.height;
        } else {
            const auto& scr = ::ribon::gfx::getScreen();
            parentX = 0;
            parentY = 0;
            parentW = static_cast<int>(scr.width);
            parentH = static_cast<int>(scr.height);
        }

        int baseX = parentX + static_cast<int>(parentW * rel.anchorX);
        int baseY = parentY + static_cast<int>(parentH * rel.anchorY);

        return Point {
            baseX + rel.offsetX,
            baseY + rel.offsetY
        };
    }

    /**
     * @brief AbsolutePos -> Rect
     */
    inline Rect toRect(const AbsolutePos& abs, int width, int height) {
        return Rect {
            abs.x,
            abs.y,
            width,
            height
        };
    }

    /**
     * @brief RelativePos -> Rect
     */
    inline Rect toRect(const RelativePos& rel, int width, int height) {
        Point p = toPoint(rel);

        return Rect {
            p.x,
            p.y,
            width,
            height
        };
    }

} // namespace ribon::coord::transform
