// src/ui/render.cpp
#include <Ribon/Ui.hpp>
#include <Ribon/InputSystem.hpp>
#include <Ribon/Time.hpp>

#include "../gfx/doublebuffer.hpp"

void ribon::ui::runUiLoop(Widget** roots, size_t rootCount, const UiLoopConfig& cfg) {
    using namespace ribon;

    // 1) 입력 시스템 초기화
    auto& input = IO::GetInput();
    input.init();

    // 2) FPS 설정 (0이면 60으로 고정)
    uint32_t fps = cfg.targetFps ? cfg.targetFps : 60;
    if (fps == 0) fps = 60;
    const uint64_t frameUsec = 1000000ull / fps;

    bool dirty = true;  // 첫 프레임은 반드시 그리기

    while (1) {
        // 프레임 시작 시각
        const uint64_t frameStart = nowMicroseconds();

        // 입력 업데이트
        input.update();
        const auto key = input.getKey(); // 복사본

        if (key.pressed) {
            bool changed = false;

            // 방향키 -> 포커스 이동
            switch (key.scanCode) {
                case SCAN_RIGHT:
                    FocusMove(FocusDirection::Right);
                    changed = true;
                    break;
                case SCAN_LEFT:
                    FocusMove(FocusDirection::Left);
                    changed = true;
                    break;
                case SCAN_UP:
                    FocusMove(FocusDirection::Up);
                    changed = true;
                    break;
                case SCAN_DOWN:
                    FocusMove(FocusDirection::Down);
                    changed = true;
                    break;
                default:
                    break;
            }

            // 엔터 -> 활성화
            if (key.unicode == CHAR_CARRIAGE_RETURN) {
                FocusActivate();
                changed = true;
            }

            if (changed) {
                dirty = true;   // 이번 프레임은 다시 그려야 함
            }
        }

        // 2) 화면이 바뀔 필요가 있을 때만 렌더링
        if (dirty) {
            gfx::clear(cfg.bg_r, cfg.bg_g, cfg.bg_b, cfg.bg_a);

            for (size_t i = 0; i < rootCount; ++i) {
                renderSingleUi(roots[i]);
            }

            // 더블 버퍼링이 켜져 있으면 백버퍼 -> 실제 GOP로 복사
            // 꺼져 있으면 present()는 내부에서 바로 return
            gfx::present();

            dirty = false;
        }

        const uint64_t frameEnd = nowMicroseconds();
        uint64_t elapsed = 0;

        if (frameEnd >= frameStart) {
            elapsed = frameEnd - frameStart;
        }

        if (elapsed < frameUsec) {
            sleepMicroseconds(frameUsec - elapsed);
        }
        // 만약 elapsed >= frameUsec 이면 렌더링이 느린 것이므로
        // 추가 Stall 없이 바로 다음 프레임으로 진행

    }
}
