// panel.hpp
#pragma once

#include "widget.hpp"
#include <Ribon/ui/coord/renderer.hpp>

namespace ribon::ui {

    struct PanelStyle {
        // 배경색
        uint8_t bg_r, bg_g, bg_b, bg_a;

        // Border
        bool border = true;
        uint8_t border_r = 255, border_g = 255, border_b = 255, border_a = 40;

        // Shadow
        bool shadow = true;
        uint8_t shadow_a = 60; // 그림자 alpha

        // Corner radius
        uint8_t radius = 0;

        // Title bar
        bool titleBar = false;
        uint8_t title_r = 80, title_g = 80, title_b = 80, title_a = 200;
        const char* titleText = nullptr;
    };

    class Panel : public Widget {
    public:
        PanelStyle style;

        Panel() {  type = WidgetType::Panel;  }
    };

} // namespace ribon::ui