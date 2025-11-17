#pragma once

#include <stdint.h>
#include <stddef.h>

namespace ribon::gfx {

    /**
     * @brief 프레임버퍼에 알파 블렌딩을 적용하여 1픽셀을 그린다.
     *
     * @param x 픽셀 X 위치
     * @param y 픽셀 Y 위치
     * @param r 0~255  
     * @param g 0~255  
     * @param b 0~255 
     * @param a 0~255  (알파값. 0=완전투명, 255=불투명)
     */
    void drawPixelAlpha(
        int x, int y,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a
    );

    /**
     * @brief 알파 블렌딩이 적용된 단색 사각형을 그린다.
     *
     * @param x 시작 X
     * @param y 시작 Y
     * @param w 폭
     * @param h 높이
     * @param r 색상 R
     * @param g 색상 G
     * @param b 색상 B
     * @param a 알파(0~255)
     */
    void drawRectAlpha(
        int x, int y, int w, int h,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a
    );

    /**
     * @brief 이미 RGBA32 포맷으로 준비된 픽셀 버퍼를
     *        알파 블렌딩하여 화면에 그린다.
     *        (PNG 렌더러가 완성되면 이 함수를 사용)
     *
     * @param px RGBA8888 배열 (row-major)
     * @param sx 화면에 찍을 X
     * @param sy 화면에 찍을 Y
     * @param w 폭
     * @param h 높이
     */
    void drawImageRGBA(
        const uint32_t* px,
        int sx, int sy,
        int w, int h
    );

} // namespace ribon::gfx