#include <Ribon/ui/widget.hpp>
#include <Ribon/ui/Focus.hpp>
#include <Ribon/ui/button.hpp>
#include <Ribon/ui/panel.hpp>

namespace ribon::ui::detail {

    constexpr size_t MAX_FOCUS_WIDGETS = 64;

    Widget* g_focusables[MAX_FOCUS_WIDGETS];
    Widget* g_focused    = nullptr;
    size_t  g_focusCount = 0;

    constexpr int centerX(const Widget* w) {
        return w->rect.x + w->rect.width / 2;
    }

    constexpr int centerY(const Widget* w) {
        return w->rect.y + w->rect.height / 2;
    }

    constexpr bool isInDirection(FocusDirection dir, int dx, int dy) {
        switch (dir) {
            case FocusDirection::Right: return dx > 0;
            case FocusDirection::Left:  return dx < 0;
            case FocusDirection::Down:  return dy > 0;
            case FocusDirection::Up:    return dy < 0;
        }
        return false;
    }

    Widget* findNext(Widget* current, FocusDirection dir) {
        if (!current || g_focusCount == 0) return nullptr;

        const int cx = centerX(current);
        const int cy = centerY(current);

        Widget* best   = nullptr;
        long    bestD2 = 0;

        for (size_t i = 0; i < g_focusCount; ++i) {
            Widget* cand = g_focusables[i];
            if (!cand || cand == current) continue;
            if (!cand->isVisible) continue;

            const int tx = centerX(cand);
            const int ty = centerY(cand);

            const int dx = tx - cx;
            const int dy = ty - cy;

            if (!isInDirection(dir, dx, dy))
                continue;

            const long d2 = (long)dx * dx + (long)dy * dy;

            if (!best || d2 < bestD2) {
                best   = cand;
                bestD2 = d2;
            }
        }

        return best;
    }

    void syncFocusStates() {
        for (size_t i = 0; i < g_focusCount; ++i) {
            Widget* w = g_focusables[i];
            if (!w) continue;

            w->hasFocus = (w == g_focused);

            // 타입별로 추가 반응 (지금은 Button만)
            if (w->type == WidgetType::Button) {
                auto* b = static_cast<Button*>(w);
                b->isHovered = w->hasFocus;
                if (!w->hasFocus) {
                    b->isPressed = false;
                }
            }
            // Panel, Label 등은 추후 여기서 확장
        }
    }

} // namespace ribon::ui::detail

namespace ribon::ui {

    void FocusRegister(Widget* w) {
        using namespace ribon::ui::detail;
        if (!w) return;

        // 중복 등록 방지
        for (size_t i = 0; i < g_focusCount; ++i) {
            if (g_focusables[i] == w) return;
        }

        if (g_focusCount < MAX_FOCUS_WIDGETS) {
            g_focusables[g_focusCount++] = w;
        }

        // 첫 번째 등록된 위젯은 자동 포커스 후보
        if (!g_focused) {
            g_focused = w;
            syncFocusStates();
        }
    }

    void FocusUnregister(Widget* w) {
        using namespace ribon::ui::detail;
        if (!w) return;

        for (size_t i = 0; i < g_focusCount; ++i) {
            if (g_focusables[i] == w) {
                // 뒤에서 하나 당겨옴
                g_focusables[i] = g_focusables[g_focusCount - 1];
                g_focusables[g_focusCount - 1] = nullptr;
                --g_focusCount;
                break;
            }
        }

        if (g_focused) {
            g_focused = nullptr;
            if (g_focusCount > 0) {
                g_focused = g_focusables[0];
            }

            syncFocusStates();
        }
    }

    void FocusSet(Widget* w) {
        detail::g_focused = w;
        detail::syncFocusStates();
    }

    Widget* FocusGet() {
        return detail::g_focused;
    }

    void FocusMove(FocusDirection dir) {
        using namespace ribon::ui::detail;

        if (!g_focused) {
            if (g_focusCount > 0) {
                g_focused = g_focusables[0];
                syncFocusStates();
            }
            return;
        }

        Widget* next = findNext(g_focused, dir);
        if (next) {
            g_focused = next;
            syncFocusStates();
        }
    }

    void FocusActivate() {
        using namespace ribon::ui::detail;
        if (!g_focused) return;

        // 타입별로 "활성화" 동작 정의
        switch (g_focused->type) {
            case WidgetType::Button: {
                auto* b = static_cast<Button*>(g_focused);
                b->isPressed = true;
                if (b->onClick) {
                    b->onClick(b->userData);
                }
                break;
            }
            case WidgetType::Panel:
                // 패널 포커스 활성화 동작은 나중에 필요하면 정의
                break;
            default:
                break;
        }
    }



} // namespace ribon::ui