// src/ui/widget.cpp
#include <Ribon/ui/widget.hpp>
#include <Ribon/ui/panel.hpp>

#include <Ribon/Draw.hpp>
#include <Ribon/Print.hpp>
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/Math.hpp>

namespace ribon::ui::detail {

    // r > 0 인 라운드 사각형(채우기 + Wu 스타일 코너 AA)
    static void drawRoundedRectAA(
        int x, int y, int w, int h, int r,
        uint8_t R, uint8_t G, uint8_t B, uint8_t A
    ) {
        using namespace ribon::gfx;

        if (r <= 0) {
            drawRectAlpha(x, y, w, h, R, G, B, A);
            return;
        }

        // --- 1) 내부 영역: 겹치지 않게 3개의 직사각형으로 채우기 ---

        // 가운데 몸통
        drawRectAlpha(
            x + r, y,
            w - 2 * r, h,
            R, G, B, A
        );

        // 왼쪽 세로 띠
        if (h > 2 * r) {
            drawRectAlpha(
                x, y + r,
                r, h - 2 * r,
                R, G, B, A
            );
            // 오른쪽 세로 띠
            drawRectAlpha(
                x + w - r, y + r,
                r, h - 2 * r,
                R, G, B, A
            );
        }

        // --- 2) 네 모서리 AA ---

        for (int yy = 0; yy < r; ++yy) {
            for (int xx = 0; xx < r; ++xx) {
                float fx = (float)(r - xx - 0.5f);
                float fy = (float)(r - yy - 0.5f);

                float dist = math::sqrtf(fx * fx + fy * fy);
                float diff = (float)r - dist;   // r보다 얼마나 안쪽인지

                if (diff <= 0.0f) {
                    // 원 바깥 → 안 칠함
                    continue;
                }

                uint8_t a2;
                if (diff >= 1.0f) {
                    a2 = A;                    // 완전 내부
                } else {
                    a2 = (uint8_t)(A * diff);  // 경계 근처 → 알파 스케일
                }

                // TL
                drawPixelAlpha(x + xx,          y + yy,          R, G, B, a2);
                // TR
                drawPixelAlpha(x + w - xx - 1,  y + yy,          R, G, B, a2);
                // BL
                drawPixelAlpha(x + xx,          y + h - yy - 1,  R, G, B, a2);
                // BR
                drawPixelAlpha(x + w - xx - 1,  y + h - yy - 1,  R, G, B, a2);
            }
        }
    }


    // 위쪽만 둥근 원 - Title bar 전용
    static void drawTopRoundedBarAA(
        int x, int y, int w, int h, int r,
        uint8_t R, uint8_t G, uint8_t B, uint8_t A
    ) {
        using namespace ribon::gfx;

        if (h <= 0) return;

        if (r <= 0) {
            drawRectAlpha(x, y, w, h, R, G, B, A);
            return;
        }

        // 1) 상단 r 라인까지: 가운데 + 코너 AA
        int cornerH = (h < r) ? h : r;

        // 가운데 직사각형 (코너 제외) - 상단 r 라인까지만
        drawRectAlpha(
            x + r, y,
            w - r * 2, cornerH,
            R, G, B, A
        );

        // 코너는 위쪽 두 개만 사용
        for (int yy = 0; yy < cornerH; ++yy) {
            for (int xx = 0; xx < r; ++xx) {
                float fx = (float)(r - xx - 0.5f);
                float fy = (float)(r - yy - 0.5f);

                float dist = math::sqrtf(fx * fx + fy * fy);
                float diff = (float)r - dist;

                if (diff <= 0.0f) continue;

                uint8_t a2 = (diff >= 1.0f)
                    ? A
                    : (uint8_t)(A * diff);

                // TL
                drawPixelAlpha(x + xx,         y + yy, R, G, B, a2);
                // TR
                drawPixelAlpha(x + w - xx - 1, y + yy, R, G, B, a2);
            }
        }

        // 2) 높이가 r 보다 크면, 아래쪽 나머지 부분은 전체 폭으로 채우기
        if (h > r) {
            drawRectAlpha(
                x, y + cornerH,
                w, h - cornerH,
                R, G, B, A
            );
        }
    }


} // namespace ribon::ui::detail

namespace ribon::ui {

    void drawPanel(const Widget& w) {
        using namespace ribon;

        const Panel&      p  = static_cast<const Panel&>(w);
        const PanelStyle& st = p.style;

        const int x   = w.rect.x;
        const int y   = w.rect.y;
        const int wdt = w.rect.width;
        const int hgt = w.rect.height;
        const int r   = st.radius;

        // ----------------------------------------------------
        // 1) Shadow : 오른쪽 아래로 더 크게 이동
        // ----------------------------------------------------
        if (st.shadow) {
            detail::drawRoundedRectAA(
                x + 6, y + 6,   // 이전 3,3 → 6,6
                wdt, hgt, r,
                0, 0, 0, st.shadow_a
            );
        }

        // ----------------------------------------------------
        // 2) Panel 본체 배경
        // ----------------------------------------------------
        detail::drawRoundedRectAA(
            x, y, wdt, hgt, r,
            st.bg_r, st.bg_g, st.bg_b, st.bg_a
        );

        // ----------------------------------------------------
        // 3) Title Bar (있다면)
        //    -> 윗쪽 코너까지 밝은 회색으로 채워줌
        // ----------------------------------------------------
        if (st.titleBar) {
            const int tH = 24;

            detail::drawTopRoundedBarAA(
                x, y, wdt, tH, r,
                st.title_r, st.title_g, st.title_b, st.title_a
            );

            // 나중에 UI 전용 Print 모드로 제목을 여기 안에 렌더하면 됨.
        }

        // ----------------------------------------------------
        // 4) Border (맨 마지막, 내부 1px 라인만)
        // ----------------------------------------------------
        /* NOPE
        if (st.border && r == 0) {
            // radius 없는 박스에만 보더 적용
            gfx::drawRectAlpha(
                x, y, wdt, 1,
                st.border_r, st.border_g, st.border_b, st.border_a
            );
            gfx::drawRectAlpha(
                x, y + hgt - 1, wdt, 1,
                st.border_r, st.border_g, st.border_b, st.border_a
            );
            gfx::drawRectAlpha(
                x, y, 1, hgt,
                st.border_r, st.border_g, st.border_b, st.border_a
            );
            gfx::drawRectAlpha(
                x + wdt - 1, y, 1, hgt,
                st.border_r, st.border_g, st.border_b, st.border_a
            );
        }
        */
    
    }

    void drawLabel(const Widget& w) {
        using namespace ribon;
        
        IO::Print("%s", "Label");
    }

    void drawButton(const Widget& w) {
        using namespace ribon;
        
        const int x = w.rect.x;
        const int y = w.rect.y;
        const int wdt = w.rect.width;
        const int hgt = w.rect.height;

        // 약간 어두운 회색 버튼 배경
        gfx::drawRectAlpha(
            x, y, wdt, hgt,
            120, 120, 120, 255
        );

        // 텍스트 출력(중앙 정렬 X, 단순 offset)
        IO::Print("%s", "Button");
    }

    void drawWidget(const Widget& w) {
        if (!w.isVisible) return;

        switch (w.type) {
            case WidgetType::Panel:
                drawPanel(w);
                break;

            case WidgetType::Label:
                drawLabel(w);
                break;

            case WidgetType::Button:
                drawButton(w);
                break;
        }

        // child rendering
        for (size_t i = 0; i < w.childCount; ++i) {
            drawWidget(*w.children[i]);
        }
    }



} // namespace ribon::ui