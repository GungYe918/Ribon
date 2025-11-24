// include/ui/ImageFrame.hpp
#pragma once
#include "widget.hpp"
#include <Ribon/Image.hpp>
#include <Ribon/Memory.hpp>
#include <Ribon/String.hpp>
#include <Ribon/Init.hpp>

namespace ribon::ui {

    struct IFrameStyle {
        bool rounded = false;   // radius 적용 여부
        uint8_t radius = 0;

        // 배경(이미지가 작거나 빈 공간 있을 경우)
        uint8_t bg_r = 0, bg_g = 0, bg_b = 0, bg_a = 0;

        // 테두리 옵션
        bool border = false;
        uint8_t border_r = 255, border_g = 255, border_b = 255, border_a = 255;
    };

    class IFrame : public Widget {
    public:
        IFrameStyle style;
        ribon::gfx::Image image;

        IFrame() {
            type = WidgetType::IFrame;
        }
    };

    inline IFrame* createIFrame(
        int x, int y, int w, int h,
        const CHAR16* path
    ) {
        IFrame* f = new IFrame();
        if (!f) return nullptr;

        f->rect = { x, y, w, h };
        f->parent = nullptr;
        f->childCount = 0;
        f->isVisible = true;

        // 완전 투명 배경
        f->style.bg_r = f->style.bg_g = f->style.bg_b = 0;
        f->style.bg_a = 0;
        f->style.rounded = false;

        if (path) {
            EFI_FILE_PROTOCOL* root = ribon::IO::getFileRoot();
            if (root) {
                f->image = ribon::gfx::Image::loadFromFile(root, path);
            }
        }

        return f;
    }



} // namespace ribon::ui