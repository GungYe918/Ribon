#pragma once

#include "widget.hpp"
#include <Ribon/ui/coord/renderer.hpp>

namespace ribon::ui {

    struct Button {
        const char* text;

        uint8_t r, g, b, a;
        uint8_t hr, hg, hb, ha;     // hover
        uint8_t pr, pg, pb, pa;     // pressed

        bool isHovered;
        bool isPressed;

        // onClick 같은 동작은 위젯 시스템 외부에서 수행
        void (*onClick)(void* user);
        void* userData;
    };

} // namespace ribon::ui