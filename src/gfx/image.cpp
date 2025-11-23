// src/gfx/image.cpp
#include <Ribon/Image.hpp>

#include <Ribon/base/stb_config.h>
#include <Ribon/base/stb_image.h>

namespace ribon::gfx {

    void Image::unload() {
        if (data) {
            stbi_image_free(data);
            data = nullptr;
        }
    }

    Image Image::loadFromMemory(const void* buffer, size_t size) {
        Image img;

        int w, h, ch;
        unsigned char* decoded =
            stbi_load_from_memory(
                (const unsigned char*)buffer,
                (int)size,
                &w, &h, &ch,
                4      // 강제 RGBA8888
            );

        if (!decoded) {
            ribon::IO::Print<ribon::IO::Tags::DEBUG>(
                "stb_image failed: %s\n", stbi_failure_reason()
            );
            return img; // invalid
        }

        img.data = decoded;
        img.width = w;
        img.height = h;
        img.channels = 4;

        return img;
    }

    // UEFI 파일에서 로드
    Image Image::loadFromFile(EFI_FILE_PROTOCOL* root, const CHAR16* path) {
        EFI_FILE_PROTOCOL* file = nullptr;

        EFI_STATUS st = root->Open(
            root, &file, (CHAR16*)path,
            EFI_FILE_MODE_READ, 0
        );
        if (EFI_ERROR(st)) {
            ribon::IO::Print("[DBG] Failed to open file: %ls (st=0x%lx)\n", path, st);
            return Image{};
        }

        // ------------------------------
        // 1) 파일 크기 정확히 읽기
        // ------------------------------
        UINTN infoSize = 0;
        st = file->GetInfo(file, &gEfiFileInfoGuid, &infoSize, nullptr);
        if (st != EFI_BUFFER_TOO_SMALL) {
            ribon::IO::Print("[ERR] GetInfo size query failed\n");
            file->Close(file);
            return Image{};
        }

        EFI_FILE_INFO* info = nullptr;
        ribon::mem::AllocateZeroPool(EfiLoaderData, infoSize, (void**)&info);

        st = file->GetInfo(file, &gEfiFileInfoGuid, &infoSize, info);
        if (EFI_ERROR(st)) {
            ribon::IO::Print("[ERR] GetInfo second call failed\n");
            ribon::mem::FreePool(info);
            file->Close(file);
            return Image{};
        }

        UINTN fileSize = info->FileSize;
        ribon::mem::FreePool(info);

        // ------------------------------
        // 2) 파일 내용 읽기
        // ------------------------------
        void* buf = nullptr;
        ribon::mem::AllocateZeroPool(EfiLoaderData, fileSize, &buf);
        UINTN readSize = fileSize;

        st = file->Read(file, &readSize, buf);
        file->Close(file);

        if (EFI_ERROR(st) || readSize != fileSize) {
            ribon::IO::Print("[ERR] Read failed: read=%lu expected=%lu\n", 
                            readSize, fileSize);
            ribon::mem::FreePool(buf);
            return Image{};
        }

        // ------------------------------
        // 3) PNG 디코딩
        // ------------------------------
        Image img = loadFromMemory(buf, fileSize);

        ribon::mem::FreePool(buf);
        return img;
    }


    void drawImage(int dstX, int dstY, const Image& image) {
        if (!image.data || image.width == 0 || image.height == 0) {
            return;
        }

        // stb_image 결과는 RGBA8888 바이트 배열
        // drawImageRGBA는 RGBA8888(R,G,B,A) 버퍼로 동작하도록 구현됨
        drawImageRGBA(
            reinterpret_cast<const uint32_t*>(image.data),
            dstX,
            dstY,
            image.width,
            image.height
        );
    }


} // namespace ribon::gfx