// Ribon/ui/coord/rect.hpp
#pragma once
#include "point.hpp"

/**
 * @file rect.hpp
 * @brief UI 요소의 경계(사각형 영역)를 정의
 */

namespace ribon::coord {

    /**
     * @struct Rect
     * @brief UI 컨테이너/위젯의 사각형 영역
     *
     * x, y는 좌상단 절대 좌표이며 width, height는 크기이다.
     * UI 레이아웃 엔진에서 위젯의 실제 좌표가 이 구조체로 계산되어 저장된다.
     */
    struct Rect {
        int x;      ///< 좌상단 X 좌표
        int y;      ///< 좌상단 Y 좌표
        int width;  ///< 가로 크기
        int height; ///< 세로 크기
    };

} // namespace ribon::coord