// Ribon/ui/Ui.hpp
#pragma once

#include "Coord.hpp"

#include "ui/button.hpp"
#include "ui/label.hpp"
#include "ui/layout.hpp"
#include "ui/panel.hpp"
#include "ui/widget.hpp"
#include "ui/ImageFrame.hpp"

extern "C" {
    #include <Uefi.h>
}

namespace ribon::ui {

    struct UiLoopConfig {
        // 배경 색
        uint8_t bg_r;
        uint8_t bg_g;
        uint8_t bg_b;
        uint8_t bg_a;

        // 목표 FPS (0이면 60으로 가정)
        uint32_t targetFps;

        constexpr UiLoopConfig()
            : bg_r(0), bg_g(0), bg_b(0), bg_a(255),
              targetFps(60)
        {}
    };

    inline void renderSingleUi(Widget* root) {
        if (!root) return;
        drawWidget(*root);
    }

    void runUiLoop(Widget** roots, size_t rootCount, const UiLoopConfig& cfg);

} // namespace ribon::ui