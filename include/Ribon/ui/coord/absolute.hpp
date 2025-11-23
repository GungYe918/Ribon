// Ribon/ui/coord/absolute.hpp
#pragma once


/**
 * @file absolute.hpp
 * @brief 절대 좌표 기반 위치 설정용 구조체 (POD)
 */

namespace ribon::coord {

    /**
     * @struct AbsolutePos
     * @brief 화면 절대 좌표 기반 위치.
     *
     * 해상도가 변해도 동일한 픽셀 기반 좌표를 가정한다.
     * (Ex: 버튼을 항상 50, 50 위치에 둔다)
     */
    struct AbsolutePos {
        int x; ///< 절대 X 좌표
        int y; ///< 절대 Y 좌표
    };

} // namespace ribon::coord