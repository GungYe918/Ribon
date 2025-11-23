// include/ui/layout.hpp
#pragma once
#include "widget.hpp"

namespace ribon::ui {

    enum class LayoutType {
        Vertical,
        Horizontal
    };

    class Layout : public Widget {
    public:
        LayoutType layout;
        int spacing;

        Layout() {
            type = WidgetType::Layout;
            spacing = 6;
            layout = LayoutType::Vertical;
        }
    };

    inline Layout* createLayout(int x, int y, int w, int h, LayoutType tp) {
        Layout* ly = new Layout();
        ly->rect = { x, y, w, h };
        ly->layout = tp;
        ly->isVisible = true;
        return ly;
    }

} // namespace ribon::ui