// src/ui/render.cpp
#include <Ribon/Ui.hpp>
#include <Ribon/InputSystem.hpp>
#include <Ribon/Time.hpp>

#include "../gfx/doublebuffer.hpp"

void ribon::ui::runUiLoop(const UiLoopConfig& cfg) {
    using namespace ribon;

    // 이 둘은 루프 밖에서 가져와도 되지만,
    // rootCount는 매 프레임 갱신해야 한다.
    auto roots = ribon::ui::getRootWidgetList();

    // 1) 입력 시스템 초기화
    auto& input = IO::GetInput();
    input.init();

    uint32_t fps = cfg.targetFps ? cfg.targetFps : 60;
    if (fps == 0) fps = 60;
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

        if (dirty) {
            gfx::clear(cfg.bg_r, cfg.bg_g, cfg.bg_b, cfg.bg_a);

            // ★ 여기에서 매 프레임마다 최신 개수 읽기
            size_t rootCount = ribon::ui::getRootWidgetCount();

            for (size_t i = 0; i < rootCount; ++i) {
                renderSingleUi(roots[i]);
            }

            gfx::present();
            dirty = false;
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
