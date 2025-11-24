#pragma once

extern "C" {
    #include <Uefi.h>
    #include <Protocol/SimplePointer.h>
    #include <Protocol/SimpleTextInEx.h>
}

#include <Ribon/Init.hpp>

namespace ribon::IO {

    struct MouseState {
        INT64 x;
        INT64 y;
        bool left;
        bool right;
    };

    struct KeyState {
        bool pressed;
        UINT16 unicode;      // 입력 문자
        UINT16 scanCode;     // UEFI 스캔코드
        bool shift;
        bool ctrl;
        bool alt;
    };

    // Input Class
    class InputSystem {
    public:
        constexpr InputSystem() = default;

        bool init() {
            EFI_STATUS st;
            auto sBS = ribon::getBS();
            
            // ------------------------
            //   Pointer (마우스)
            // ------------------------
            st = sBS->LocateProtocol(
                &gEfiSimplePointerProtocolGuid,
                nullptr,
                (void**)&pointer
            );
            if (EFI_ERROR(st)) pointer = nullptr;
            else pointerWaitEvent = pointer -> WaitForInput;

            // ------------------------
            //   Keyboard (TextInEx)
            // ------------------------
            st = sBS->LocateProtocol(
                &gEfiSimpleTextInputExProtocolGuid,
                nullptr,
                (void**)&textEx
            );
            if (EFI_ERROR(st)) textEx = nullptr;
            else keyboardWaitEvent = textEx -> WaitForKeyEx;

            // 초기 좌표 (0,0)
            mouse.x = mouse.y = 0;
            mouse.left = mouse.right = false;

            key = {};
            return true;
        }

        // Poll 입력 업데이트
        void update() {
            pollMouse();
            pollKeyboard();
        }

        constexpr const MouseState& getMouse()  const       {  return mouse; }
        constexpr const KeyState&   getKey()    const       {  return key;   }
    
    private:
        EFI_SIMPLE_POINTER_PROTOCOL* pointer = nullptr;
        EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL* textEx = nullptr;

        MouseState mouse{};
        KeyState   key{};

        EFI_EVENT pointerWaitEvent = nullptr;
        EFI_EVENT keyboardWaitEvent = nullptr;

        // 마우스 이벤트 Polling
        void pollMouse() {
            if (!pointer) return;

            EFI_SIMPLE_POINTER_STATE st;
            EFI_STATUS r = pointer->GetState(pointer, &st);

            // 에러 상황에서 함수 종료
            if (EFI_ERROR(r)) return;

            // dx/dy 누적 -> 절대 좌표로 변환
            mouse.x += st.RelativeMovementX;
            mouse.y += st.RelativeMovementY;

            mouse.left  = (st.LeftButton  != 0);
            mouse.right = (st.RightButton != 0);
        }

        // 키보드 이벤트 Polling
        void pollKeyboard() {
            if (!textEx) return;

            // 기본값: 이번 프레임에는 키 입력 없음
            key.pressed = false;
            key.unicode = 0;
            key.scanCode = 0;
            key.shift = key.ctrl = key.alt = false;

            EFI_KEY_DATA kd{};
            EFI_STATUS st;
            bool any = false;

            // 입력 버퍼를 한 프레임에 전부 비움
            while (true) {
                st = textEx->ReadKeyStrokeEx(textEx, &kd);
                if (st == EFI_NOT_READY) {
                    // 더 읽을 키 없음
                    break;
                }
                if (EFI_ERROR(st)) {
                    // 에러면 종료 |  이번 프레임은 "입력 없음"으로 설정
                    break;
                }

                any = true;

                // 마지막 이벤트만 남기기
                key.pressed  = true;
                key.unicode  = kd.Key.UnicodeChar;
                key.scanCode = kd.Key.ScanCode;

                auto s = kd.KeyState.KeyShiftState;
                if (s & EFI_SHIFT_STATE_VALID) {
                    key.shift = (s & (EFI_LEFT_SHIFT_PRESSED    | EFI_RIGHT_SHIFT_PRESSED))     != 0;
                    key.ctrl  = (s & (EFI_LEFT_CONTROL_PRESSED  | EFI_RIGHT_CONTROL_PRESSED))   != 0;
                    key.alt   = (s & (EFI_LEFT_ALT_PRESSED      | EFI_RIGHT_ALT_PRESSED))       != 0;
                }
            }

            if (!any) {
                key.pressed = false;
            }
        }
            
    };

    inline InputSystem& GetInput() {
        static InputSystem instance{};
        return instance;
    }

} // namespace ribon::IO