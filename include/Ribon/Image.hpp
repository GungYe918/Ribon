// include/Image.hpp
#pragma once

#include <Ribon/Memory.hpp>
#include <stddef.h>
#include <stdint.h>

#include <Ribon/FrameBuffer.hpp>
#include <Ribon/Draw.hpp>
#include <Ribon/Print.hpp>
#include <Ribon/File.hpp>


extern "C" {
    #include <Uefi.h>
    #include <Protocol/SimpleFileSystem.h>    
}

namespace ribon::gfx {

    class Image {
    public:
        int width = 0;
        int height = 0;
        int channels = 0;  // 항상 4(RGBA)로 강제 변환
        unsigned char* data = nullptr;

        Image() = default;

        // 메모리 해제
        ~Image() {
            unload();
        }
        
        bool isValid() const {
            return (data != nullptr);
        }

        static Image loadFromFile(EFI_FILE_PROTOCOL* root, const CHAR16* path);
        static Image loadFromMemory(const void* buffer, size_t size);
    
        void unload();
    
    };

    // 프레임버퍼에 이미지 그리기
    void drawImage(int x, int y, const Image& img);

} // namespace ribon::gfx