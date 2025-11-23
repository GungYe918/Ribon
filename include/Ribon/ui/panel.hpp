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

    inline Panel* createPanel(int x, int y, int w, int h) {
        Panel* p = new Panel();
        if (!p) return nullptr;

        p->rect = {  x, y, w, h  };
        p->parent = nullptr;
        p->childCount = 0;
        p->isVisible = true;

        // 기본 스타일
        p->style.bg_r = 40;
        p->style.bg_g = 40;
        p->style.bg_b = 40;
        p->style.bg_a = 220;
        p->style.radius = 10;
        p->style.border = true;
        p->style.shadow = true;

        return p;
    }

} // namespace ribon::ui