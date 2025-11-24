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
        Layout,
        IFrame,
    };

    class Widget {
    public:
        WidgetType type;
        ribon::coord::Rect rect;

        Widget* parent;
        Widget* children[8];
        size_t childCount;

        bool isVisible;
        bool hasFocus;  // 이 위젯이 Focus기능을 가지고 있는가 

        Widget()
            : type(WidgetType::Panel),
              rect{0, 0, 0, 0},
              parent(nullptr),
              childCount(0),
              isVisible(true),
              hasFocus(false)
        {
            for (size_t i = 0; i < 8; ++i) {
                children[i] = nullptr;
            }
        }
    };


    // --------------
    // Draw API
    // --------------

    // 최상위 디스패처
    void drawWidget(const Widget& w);

    // ----------------
    // 개별 위젯 함수들 
    // ----------------

    void drawPanel(const Widget& w);
    void drawLabel(const Widget& w);
    void drawButton(const Widget& w);
    void drawIFrame(const Widget& w);

} // namespace ribon::ui