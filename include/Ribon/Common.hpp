#pragma once

#include <stdint.h>

namespace ribon {
     
    enum class RenderMode : uint8_t {
        SingleBuffer = 0,
        DoubleBuffer = 1
    };

    struct CommonConfig {
        // 기본 배경색
        uint8_t bg_r = 0;
        uint8_t bg_g = 0;
        uint8_t bg_b = 0;
        uint8_t bg_a = 255;

        // 렌더링 모드
        RenderMode renderMode = RenderMode::DoubleBuffer;

        // 성능 최적화 모드(예: animation 강화 시 false)
        bool perfMode = true;

        // 데스크탑 스타일 UI인지 여부
        bool desktopEnvironment = false;

        // 화면비 관련 기본 정보 (원한다면 더 확장 가능)
        uint8_t width = 0;
        uint8_t height = 0;
    };

    // 전역 config 접근
    CommonConfig& GetCommonConfig();


} // namespace ribon