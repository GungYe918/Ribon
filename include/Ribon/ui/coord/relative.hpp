// Ribon/ui/coord/relative.hpp
#pragma once

#include "rect.hpp"

/**
 * @file relative.hpp
 * @brief 부모 Rect를 기준으로 상대적 위치를 계산하는 구조체 (POD)
 */

namespace ribon::coord {
     
    /**
     * @struct RelativePos
     * @brief 부모 위젯의 Rect를 기준으로 비율 기반 위치를 표현.
     *
     * anchorX, anchorY는 0.0 ~ 1.0 범위의 상대 위치.
     * offsetX, offsetY는 추가 픽셀 오프셋.
     *
     * 예: anchorX=0.5, anchorY=0.5 -> 부모의 정중앙
     */
    struct RelativePos {
        const Rect* parent; ///< 부모 Rect (nullptr이면 화면 전체를 의미)
        float anchorX;      ///< 부모 width 비율 (0.0 ~ 1.0)
        float anchorY;      ///< 부모 height 비율 (0.0 ~ 1.0)
        int offsetX;        ///< X 방향 픽셀 단위 보정
        int offsetY;        ///< Y 방향 픽셀 단위 보정
    };

} // namespace ribon::coord