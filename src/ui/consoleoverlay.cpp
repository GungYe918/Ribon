#include <Ribon/ui/ConsoleOverlay.hpp>
#include <Ribon/Ui.hpp>

namespace ribon::ui {

    static ConsoleOverlay* g_consoleOverlay = nullptr;

    ConsoleOverlay* getConsoleOverlay() {
        return g_consoleOverlay;
    }

    ConsoleOverlay* setConsoleOverlay() {
        if (g_consoleOverlay) return g_consoleOverlay;

        ConsoleOverlay* ov = new ConsoleOverlay();
        if (!ov) return nullptr;

        ov->rect = { 0, 0, 0, 0 }; // 위치/크기는 무의미, 항상 화면 전체에 로그처럼 씀

        registerRootWidget(ov);
        g_consoleOverlay = ov;
        return ov;
    }

    void ConsoleOverlay::append(const CHAR16* text) {
        if (!text) return;

        if (lineCount == 0) {
            lineCount = 1;
            lines[0]  = ribon::str::Utf16String();
        }

        size_t idx = lineCount - 1;

        for (size_t i = 0; text[i] != 0; ++i) {
            CHAR16 c = text[i];

            if (c == L'\r') {
                continue;
            } else if (c == L'\n') {
                // 새 줄 생성 또는 스크롤
                if (lineCount < MAX_LINES) {
                    ++lineCount;
                } else {
                    // 스크롤 업
                    for (size_t j = 1; j < MAX_LINES; ++j)
                        lines[j - 1].copy_from_utf16(lines[j]);
                }
                idx = lineCount - 1;
                lines[idx] = ribon::str::Utf16String();
            } else {
                lines[idx].push_back(c);
            }
        }
    }

    void appendConsoleOverlayText(const CHAR16* text) {
        ConsoleOverlay* ov = setConsoleOverlay();
        if (!ov) return;

        ov->append(text);

        // Print가 새 텍스트를 추가했으니, 다음 프레임에 반드시 다시 그리도록 요청
        requestUiRedraw();
    }

} // namespace ribon::ui