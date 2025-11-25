// src/ui/widget.cpp
#include <Ribon/ui/widget.hpp>
#include <Ribon/ui/panel.hpp>
#include <Ribon/ui/button.hpp>
#include <Ribon/ui/layout.hpp>
#include <Ribon/ui/label.hpp>
#include <Ribon/ui/ImageFrame.hpp>
#include <Ribon/ui/ConsoleOverlay.hpp>

#include <Ribon/Draw.hpp>
#include <Ribon/Print.hpp>
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/Math.hpp>
#include <Ribon/Console.hpp>

#include "../console/font.hpp"

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
                    // 원 바깥 -> 안 칠함
                    continue;
                }

                uint8_t a2;
                if (diff >= 1.0f) {
                    a2 = A;                    // 완전 내부
                } else {
                    a2 = (uint8_t)(A * diff);  // 경계 근처 -> 알파 스케일
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

    static void applyLayout(const Layout& ly) {
        int cursor = 0;

        for (size_t i = 0; i < ly.childCount; ++i) {
            Widget* c = ly.children[i];

            if (ly.layout == LayoutType::Vertical) {
                c->rect.x = ly.rect.x;
                c->rect.y = ly.rect.y + cursor;
                cursor += c->rect.height + ly.spacing;
            } else {
                c->rect.x = ly.rect.x + cursor;
                c->rect.y = ly.rect.y;
                cursor += c->rect.width + ly.spacing;
            }
        }
    }


} // namespace ribon::ui::detail

namespace ribon::ui {

    void drawPanel(const Widget& w) {
        using namespace ribon;

        // ---------------------------------------------------------
        // [중요] SimpleText 모드는 UI 자체를 렌더할 수 없다
        // ---------------------------------------------------------
        auto con = console::getConsole();
        if (!con || con->mode() != console::TextMode::FBFont) {
            IO::Print<IO::Tags::DEBUG>(
                "[UI] Cannot render Panel: Console is not in FBFont mode.\n"
            );
            return;
        }

        const Panel& p  = static_cast<const Panel&>(w);
        const PanelStyle& st = p.style;

        int x   = w.rect.x;
        int y   = w.rect.y;
        int wdt = w.rect.width;
        int hgt = w.rect.height;
        int r   = st.radius;

        // Shadow
        if (st.shadow) {
            detail::drawRoundedRectAA(
                x + 6, y + 6,
                wdt, hgt, r,
                0, 0, 0, st.shadow_a
            );
        }

        // Body
        detail::drawRoundedRectAA(
            x, y, wdt, hgt, r,
            st.bg_r, st.bg_g, st.bg_b, st.bg_a
        );

        // Title Bar
        if (st.titleBar) {
            const int titleH = 24;

            detail::drawTopRoundedBarAA(
                x, y, wdt, titleH, r,
                st.title_r, st.title_g, st.title_b, st.title_a
            );

            // -----------------------------------------------
            // Title 텍스트 출력 (FREE_RAW)
            // -----------------------------------------------
            if (st.titleText && st.titleText[0] != '\0') {

                // 텍스트 위치 계산: 좌측 12px 패딩
                int tx = x + 12;

                // 세로 가운데 정렬
                int ty = y + (titleH - (int)ribon::font::FONT_HEIGHT) / 2;

                ribon::str::Utf16String title16(st.titleText);

                IO::FreePrintRawAt(tx, ty, title16.c_str());
            }
        }
    }

    void drawLabel(const Widget& w) {
        using namespace ribon;

        auto con = ribon::console::getConsole();
        if (!con) return;

        const Label& L = static_cast<const Label&>(w);
        if (!L.text) return;

        // 텍스트 UTF16 변환
        ribon::str::Utf16String u16(L.text);

        // 색상 적용
        con->setColor(L.r, L.g, L.b, L.a);

        // 좌표 기반 출력
        IO::FreePrintRawAt(
            w.rect.x,
            w.rect.y,
            u16.c_str()
        );

        // 색상 리셋
        con->resetColor();
    }

    
    void drawButton(const Widget& w) {
        using namespace ribon;

        auto& B = static_cast<const Button&>(w);

        int x = w.rect.x;
        int y = w.rect.y;
        int wdt = w.rect.width;
        int hgt = w.rect.height;

        uint8_t R, G, Bc, A;

        // hasFocus를 최우선으로 hover에 반영
        const bool hovered = B.isHovered || B.hasFocus;

        if (B.isPressed) {
            R = B.pr; G = B.pg; Bc = B.pb; A = B.pa;
        } else if (hovered) {
            R = B.hr; G = B.hg; Bc = B.hb; A = B.ha;
        } else {
            R = B.r; G = B.g; Bc = B.b; A = B.a;
        }

        gfx::drawRectAlpha(x, y, wdt, hgt, R, G, Bc, A);

        if (B.text) {
            ribon::str::Utf16String u16(B.text);

            int tx = x + (wdt - (int)ribon::font::FONT_WIDTH * (int)u16.length()) / 2;
            int ty = y + (hgt - (int)ribon::font::FONT_HEIGHT) / 2;

            IO::FreePrintRawAt(tx, ty, u16.c_str());
        }
    }

    void drawIFrame(const Widget& w) {
        IFrame& F = const_cast<IFrame&>(static_cast<const IFrame&>(w));

        // 이미지가 아직 로드되지 않았으면 지금 로드
        if (!F.loaded) {
            if (F.path) {
                EFI_FILE_PROTOCOL* root = ribon::IO::getFileRoot();
                if (root) {
                    F.image = ribon::gfx::Image::loadFromFile(root, F.path);

                    if (!F.image.isValid()) {
                        ribon::IO::Print("[IFrame] Image load failed: %c", F.path);
                        return;
                    }

                    // w/h가 0이면 PNG 크기로 자동 결정
                    if (F.rect.width == 0)  F.rect.width  = F.image.width;
                    if (F.rect.height == 0) F.rect.height = F.image.height;

                    F.loaded = true;
                }
            }
        }

        if (!F.image.isValid())
            return;

        // Rect -> Point 변환
        ribon::coord::Point pos = ribon::coord::transform::toPoint(w.rect);

        // draw
        ribon::gfx::drawImage(pos.x, pos.y, F.image);

        // 나중에 언로드
        F.image.unload();
    }

    void drawConsoleOverlay(const Widget& w) {
        using namespace ribon;

        auto con = console::getConsole();
        if (!con) return;
        if (con->mode() != console::TextMode::FBFont) {
            return;
        }

        const ConsoleOverlay& ov = static_cast<const ConsoleOverlay&>(w);

        con->setColor(ov.r, ov.g, ov.b, ov.a);

        int x = 8;
        int y = 8;
        for (size_t i = 0; i < ov.lineCount; ++i) {
            if (ov.lines[i].length() == 0) continue;

            IO::FreePrintRawAt(x, y, ov.lines[i].c_str());
            y += (int)ribon::font::FONT_HEIGHT;
        }

        con->resetColor();
    }

    void drawWidget(const Widget& w) {
        if (!w.isVisible) return;

        // Layout은 먼저 배치 처리
        if (w.type == WidgetType::Layout)
            detail::applyLayout(static_cast<const Layout&>(w));

        switch (w.type) {
            case WidgetType::Panel:             drawPanel(w);           break;
            case WidgetType::Label:             drawLabel(w);           break;
            case WidgetType::Button:            drawButton(w);          break;
            case WidgetType::IFrame:            drawIFrame(w);          break;
            case WidgetType::ConsoleOverlay:    drawConsoleOverlay(w);  break;
            case WidgetType::Layout:    /* Layout은 시각 요소 없음 */    break; 
        }

        for (size_t i = 0; i < w.childCount; ++i)
            drawWidget(*w.children[i]);
    }



} // namespace ribon::ui