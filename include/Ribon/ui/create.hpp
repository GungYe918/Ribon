#pragma once

#include <Ribon/ui/widget.hpp>
#include <Ribon/ui/panel.hpp>
#include <Ribon/Memory.hpp>

namespace ribon::ui {
     
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