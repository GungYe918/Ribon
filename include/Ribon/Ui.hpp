#pragma once

#include "Coord.hpp"

#include "ui/button.hpp"
#include "ui/label.hpp"
#include "ui/layout.hpp"
#include "ui/panel.hpp"
#include "ui/widget.hpp"
#include "ui/create.hpp"

namespace ribon::ui {

    inline void Render(Widget* root) {
        if (!root) return;
        drawWidget(*root);
    }

} // namespace ribon::ui