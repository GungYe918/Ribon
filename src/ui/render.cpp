// src/ui/render.cpp
#include <Ribon/Ui.hpp>
#include <Ribon/InputSystem.hpp>
#include <Ribon/Time.hpp>
#include <Ribon/Common.hpp>

#include "../gfx/doublebuffer.hpp"

namespace ribon::ui::detail {

    bool g_externalDirty = false;
    bool g_requestEnd    = false;

    // runUiLoop 내부에서만 사용
    bool consumeUiRedrawFlag() {
        bool v = g_externalDirty;
        g_externalDirty = false;
        return v;
    }

    bool consumeUiEndLoopFlag() {
        bool v = g_requestEnd;
        g_requestEnd = false;
        return v;
    }

} // namespace ribon::ui::detail

void ribon::ui::runUiLoop(const UiLoopConfig& cfg) {
    using namespace ribon;

    auto roots = ribon::ui::getRootWidgetList();
    auto& ccfg = ribon::GetCommonConfig();

    auto& input = IO::GetInput();
    input.init();

    uint32_t fps = cfg.targetFps ? cfg.targetFps : 60;
    const uint64_t frameUsec = 1000000ull / fps;

    bool dirty = true;

    while (1) {
        const uint64_t frameStart = nowMicroseconds();

        input.update();
        const auto key = input.getKey();

        if (key.pressed) {
            bool changed = false;

            switch (key.scanCode) {
                case SCAN_RIGHT: FocusMove(FocusDirection::Right); changed = true; break;
                case SCAN_LEFT:  FocusMove(FocusDirection::Left);  changed = true; break;
                case SCAN_UP:    FocusMove(FocusDirection::Up);    changed = true; break;
                case SCAN_DOWN:  FocusMove(FocusDirection::Down);  changed = true; break;
                default: break;
            }

            if (key.unicode == CHAR_CARRIAGE_RETURN) {
                FocusActivate();
                changed = true;
            }

            if (changed) dirty = true;
        }

        // Print/ConsoleOverlay 등에서 요청한 redraw 반영
        if (detail::consumeUiRedrawFlag()) {
            dirty = true;
        }

        if (detail::consumeUiEndLoopFlag()) {
            return;
        }

        if (dirty) {
            gfx::clear(cfg.bg_r, cfg.bg_g, cfg.bg_b, cfg.bg_a);

            size_t rootCount = ribon::ui::getRootWidgetCount();
            Widget** roots = ribon::ui::getRootWidgetList();

            // ConsoleOverlay는 나중에 따로 그리기 위해 잡아둠
            Widget* overlay = nullptr;

            for (size_t i = 0; i < rootCount; ++i) {
                Widget* w = roots[i];
                if (!w) continue;

                if (w->type == WidgetType::ConsoleOverlay) {
                    overlay = w;
                    continue;
                }

                renderSingleUi(w);
            }

            // 맨 마지막에 ConsoleOverlay 렌더링 
            if (overlay) {
                renderSingleUi(overlay);
            }

            gfx::present();
        }

        const uint64_t frameEnd = nowMicroseconds();
        uint64_t elapsed = (frameEnd >= frameStart)
                         ? (frameEnd - frameStart)
                         : 0;

        if (elapsed < frameUsec) {
            sleepMicroseconds(frameUsec - elapsed);
        }
    
    }
}

// 외부에서 호출
void ribon::ui::requestUiRedraw() {
    detail::g_externalDirty = true;
}

void ribon::ui::requestUiEndLoop() {
    detail::g_requestEnd = true;
}
