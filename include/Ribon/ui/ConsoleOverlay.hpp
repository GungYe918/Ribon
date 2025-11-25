// Ribon/ui/ConsoleOverlay.hpp
#pragma once

#include <Ribon/String.hpp>
#include "widget.hpp"

extern "C" {
    #include <Uefi.h>
}

namespace ribon::ui {

    class ConsoleOverlay : public Widget {
    public:
        static constexpr size_t MAX_LINES = 128;

        ribon::str::Utf16String lines[MAX_LINES];
        size_t lineCount;

        uint8_t r, g, b, a;

        ConsoleOverlay()
            : lineCount(0), r(200), g(200), b(200), a(255)
        {
            type      = WidgetType::ConsoleOverlay;
            isVisible = true;
        }

        void append(const CHAR16* text);
    };

    // ConsoleOverlay 접근 + 생성
    ConsoleOverlay* getConsoleOverlay();
    ConsoleOverlay* setConsoleOverlay();

    // Print 쪽에서 호출할 API
    void appendConsoleOverlayText(const CHAR16* text);

} // namespace ribon::ui