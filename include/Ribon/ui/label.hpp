#pragma once

#include "widget.hpp"
#include <Ribon/ui/coord/renderer.hpp>
#include <Ribon/Print.hpp>

namespace ribon::ui {

    struct Label {
        const char* text;
        uint8_t r, g, b;
    };
    

} // namespace ribon::ui