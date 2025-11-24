// Ribon/ui/label.hpp
#pragma once

#include "widget.hpp"
#include <Ribon/Print.hpp>
#include <Ribon/Memory.hpp>

namespace ribon::ui {

    class Label : public Widget {
    public:
        const char* text;
        uint8_t r, g, b, a;

        Label() {
            type = WidgetType::Label;
            text = nullptr;
            r = g = b = 255;
            a = 255;
        }
    };

    inline Label* createLabel(int x, int y, const char* txt) {
        Label* l = new Label();
        if (!l) return nullptr;

        l->rect = { x, y, 0, 0 };  // width/height는 텍스트 길이에 따라 자동
        l->text = txt;

        // Widget 공통 필드 초기화
        l->parent     = nullptr;
        l->childCount = 0;
        for (size_t i = 0; i < 8; ++i) {
            l->children[i] = nullptr;
        }
        l->isVisible = true;

        return l;
    }


} // namespace ribon::ui