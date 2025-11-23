// Ribon/ui/coord/point.hpp
#pragma once

/**
 * @file point.hpp
 * @brief UI 좌표계에서 사용되는 2D 포인트 구조체 (POD)
 */

namespace ribon::coord {

    /**
     * @struct Point
     * @brief 2차원 좌표계의 정수형 포인트
     * 
     */
    struct Point {
        int x;  ///< X 좌표
        int y;  ///< y 좌표
    };

} // namespace ribon::coord