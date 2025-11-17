#pragma once

#include "point.hpp"
#include "rect.hpp"
#include "absolute.hpp"
#include "relative.hpp"


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
     * 부모가 nullptr이면 (0,0,width,height)를 가정해야 한다.
     * 여기서는 width/height를 모르는 상태이므로 (0,0)만 반환.
     */
    inline Point toPoint(const RelativePos& rel) {
        if (!rel.parent) {
            // 부모가 없는 경우, 화면 전체를 parent로 취급하려면
            // UI 시스템이 override 해야 한다.
            return Point{
                static_cast<int>(rel.anchorX * 0) + rel.offsetX,
                static_cast<int>(rel.anchorY * 0) + rel.offsetY
            };
        }

        const Rect& p = *rel.parent;

        int baseX = p.x + static_cast<int>(p.width  * rel.anchorX);
        int baseY = p.y + static_cast<int>(p.height * rel.anchorY);

        return Point {
            baseX + rel.offsetX,
            baseY + rel.offsetY
        };
    }

    /**
     * @brief 부모 Rect와 AbsolutePos를 조합하여 Rect 반환 (위젯 박스 생성용)
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
     * @brief 부모 Rect와 RelativePos로 실제 위젯 Rect 계산
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
