// Ribon/ui/button.hpp
#pragma once

#include "widget.hpp"
#include "Focus.hpp"

namespace ribon::ui {

    class Button : public Widget {
    public:
        const char* text;
        uint8_t r, g, b, a;

        // hover
        uint8_t hr, hg, hb, ha;

        // pressed 
        uint8_t pr, pg, pb, pa;

        bool isHovered;
        bool isPressed;

        void (*onClick)(void*);
        void* userData;

        Button() {
            type = WidgetType::Button;
            text = nullptr;

            // 기본 회색
            r = g = b = 120; a = 255;

            // hover
            hr = 150; hg = 150; hb = 150; ha = 255;

            // pressed
            pr = 100; pg = 100; pb = 100; pa = 255;

            isHovered = false;
            isPressed = false;

            onClick = nullptr;
            userData = nullptr;

            hasFocus = false;
        }
    };

    inline Button* createButton(int x, int y, int w, int h, const char* text) {
        Button* b = new Button();
        if (!b) return nullptr;

        b->rect = { x, y, w, h };
        b->text = text;

        // 위젯 공통 필드 초기화
        b->parent       = nullptr;
        b->childCount   = 0;
        b->isVisible    = true;

        FocusRegister(b);

        return b;
    }


} // namespace ribon::ui