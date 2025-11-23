// include/Ribon/Math.hpp
#pragma once

extern "C" {
    #include <stdint.h>
}

namespace ribon::math {

    // Impl - 간단한 abs함수
    constexpr float abs(float x) {
        return (x < 0.0f) ? -x : x;
    }

    // Newton-Raphson 4회 반복을 통한 sqrtf
    constexpr float sqrtf(float s) {
        if (s <= 0.0f)
            return 0.0f;

        // 초기값: s / 32 + 1
        float x = s * 0.03125f + 1.0f;

        // 고정 반복 4회 -> 완전 결정적
        x = 0.5f * (x + s / x);
        x = 0.5f * (x + s / x);
        x = 0.5f * (x + s / x);
        x = 0.5f * (x + s / x);

        return x;
    }

} // namespace ribon::math