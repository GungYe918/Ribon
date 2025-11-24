#pragma once

#include <stdint.h>
#include <Ribon/ui/widget.hpp>

namespace ribon::ui {

    // 키보드 방향키와 매칭되는 포커스 이동 방향
    enum class FocusDirection : uint8_t {
        Left,
        Right,
        Up,
        Down
    };  

    // 포커스를 가질 수 있는 위젯 등록/해제
    void FocusRegister(Widget* w);
    void FocusUnregister(Widget* w);

    // 현재 포커스 설정 / 조회
    void FocusSet(Widget* w);
    Widget* FocusGet();

    // 방향키 기반 포커스 이동
    void FocusMove(FocusDirection dir);

    // 엔터 등으로 포커스된 위젯 "활성화" (버튼이면 onClick 호출 등)
    void FocusActivate();

} // namespace ribon::ui