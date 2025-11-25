// Ribon/ui/Ui.hpp
#pragma once

#include "Coord.hpp"

#include "ui/button.hpp"
#include "ui/label.hpp"
#include "ui/layout.hpp"
#include "ui/panel.hpp"
#include "ui/widget.hpp"
#include "ui/ImageFrame.hpp"

extern "C" {
    #include <Uefi.h>
}

namespace ribon::ui {

    // 최대 루트 위젯 개수
    constexpr size_t MAX_ROOT_WIDGETS = 64;

    // 동적 루트 리스트
    inline Widget* g_rootWidgets[MAX_ROOT_WIDGETS];
    inline size_t  g_rootCount = 0;

    // -------------------------------------------------------
    // 루트 위젯 등록 API
    // -------------------------------------------------------
    inline bool registerRootWidget(Widget* w) {
        if (!w) return false;
        if (g_rootCount >= MAX_ROOT_WIDGETS) return false;

        g_rootWidgets[g_rootCount++] = w;
        return true;
    }

    inline Widget** getRootWidgetList() {
        return g_rootWidgets;
    }

    inline size_t getRootWidgetCount() {
        return g_rootCount;
    }

    // -------------------------------------------------------
    // Print 대신 Label로 메시지를 화면에 띄우는 함수
    // -------------------------------------------------------
    inline void showMessageLabel(int x, int y, const char* msg) {
        Label* l = createLabel(x, y, msg);
        l->r = 255; l->g = 255; l->b = 0; l->a = 255;
        registerRootWidget(l);
    }


    struct UiLoopConfig {
        // 배경 색
        uint8_t bg_r;
        uint8_t bg_g;
        uint8_t bg_b;
        uint8_t bg_a;

        // 목표 FPS (0이면 60으로 가정)
        uint32_t targetFps;

        constexpr UiLoopConfig()
            : bg_r(0), bg_g(0), bg_b(0), bg_a(255),
              targetFps(60)
        {}
    };

    inline void renderSingleUi(Widget* root) {
        if (!root) return;
        drawWidget(*root);
    }

    void runUiLoop(const UiLoopConfig& cfg);


    // Rerendering 호출
    void requestUiRedraw();
    void requestUiEndLoop();

    
} // namespace ribon::ui