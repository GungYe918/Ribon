#pragma once

extern "C" {
    #include <Uefi.h>
}

#include <Ribon/Init.hpp>
#include <base/Utf16String.hpp>
#include <base/Utf8String.hpp>
#include <Ribon/FrameBuffer.hpp>

namespace ribon::console {

    /** @brief 콘솔 출력 모드 */
    enum class TextMode {
        SimpleText,     // 기존 UEFI ConsoleOut 기반
        FBFont          // FrameBuffer 폰트 기반 렌더링
    };

    /** @brief Console 시스템: 텍스트 출력 계층 */
    class Console {
    public:
        /** @brief Console 생성자 - 기본 모드: SimpleText */
        Console();

        /** @brief 출력 모드 변경 */
        void SetMode(TextMode mode);
        
        /** @brief 현재 모드 조회 */
        TextMode mode() const {  return currentMode;  }

        /** @brief UTF-16 문자열 출력 */
        void write(const ribon::str::Utf16String& str);

        /** @brief ASCII/UTF-8 문자열 출력 */
        void write(const char* str);

        /** @brief 문자열 + 포맷 출력 */
        void writef(const char* fmt, ...);

        /** @brief 커서 이동 (SimpleText 모드만) */
        void setCursor(UINTN col, UINTN row);

        /** @brief 화면 초기화 */
        void clear();

    private:
        TextMode currentMode = TextMode::SimpleText;

        // 내부 헬퍼 함수
        void writeSimpleText(const CHAR16* wstr);
        void writeFramebufferText(const CHAR16* wstr);
    };

    /** @brief Console을 시스템에 등록 */
    void setConsole(Console* c);

    /** @brief 현재 등록된 Console 반환 */
    Console* getConsole();

} // namespace ribon::console