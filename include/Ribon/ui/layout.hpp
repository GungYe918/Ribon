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
        if (!ly) return nullptr;

        ly->rect   = { x, y, w, h };
        ly->layout = tp;

        // Widget 공통 필드 초기화
        ly->parent     = nullptr;
        ly->childCount = 0;
        for (size_t i = 0; i < 8; ++i) {
            ly->children[i] = nullptr;
        }
        ly->isVisible = true;

        return ly;
    }


} // namespace ribon::ui