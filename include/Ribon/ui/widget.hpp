// include/Ribon/ui/widget.hpp
#pragma once

#include <stdint.h>
#include <stddef.h>

#include <Ribon/ui/coord/rect.hpp>
#include <Ribon/ui/coord/transform.hpp>

namespace ribon::ui {

    enum class WidgetType : uint8_t {
        Panel,
        Label,
        Button,
        Layout
    };

    class Widget {
    public: 
        WidgetType type;
        ribon::coord::Rect rect;

        Widget* parent;
        Widget* children[8];
        size_t childCount;

        bool isVisible;
    };


    // --------------
    // Draw API
    // --------------

    // 최상위 디스패처
    void drawWidget(const Widget& w);


    void drawPanel(const Widget& w);
    void drawLabel(const Widget& w);
    void drawButton(const Widget& w);

} // namespace ribon::ui