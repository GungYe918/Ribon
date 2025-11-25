// include/ui/ImageFrame.hpp
#pragma once
#include "widget.hpp"
#include <Ribon/Image.hpp>
#include <Ribon/Memory.hpp>
#include <Ribon/String.hpp>
#include <Ribon/Init.hpp>

namespace ribon::ui {

    class IFrame : public Widget {
    public:
        ribon::gfx::Image       image;
        const CHAR16*           path = nullptr;   // 파일 경로 저장
        bool                    loaded = false;   // 이미지 로드 여부 체크

        IFrame() {
            type = WidgetType::IFrame;
        }

        ~IFrame() {
            image.unload();
        }
    };

    inline IFrame* createIFrame(
        int x, int y, int w, int h,
        const CHAR16* path
    ) {
        IFrame* f = new IFrame();
        if (!f) return nullptr;

        f->rect = { x, y, w, h };
        f->parent     = nullptr;
        f->childCount = 0;
        f->isVisible  = true;
        f->hasFocus   = false;

        // PNG 경로만 저장
        f->path = path;
        f->loaded = false;

        // 실제 PNG 크기는 drawIFrame에서 결정한다.
        return f;
    }

} // namespace ribon::ui