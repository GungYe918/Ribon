// src/gfx/image.cpp
#include <Ribon/Image.hpp>

#include <Ribon/base/stb_config.h>
#include <Ribon/base/stb_image.h>

namespace ribon::gfx::detail {

    static constexpr uint8_t blend8(uint8_t src, uint8_t dst, uint8_t alpha) {
        // alpha = 0~255
        return (uint8_t)(((src * alpha) + (dst * (255 - alpha))) / 255);
    }

} // namespace ribon::gfx::detail

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

        auto fb = ribon::fb::getFramebuffer();
        if (!fb) return;

        uint8_t* fbBase = (uint8_t*)fb->base;

        for (int y = 0; y < image.height; y++) {
            int fbY = dstY + y;
            if (fbY < 0 || fbY >= (int)fb->height)
                continue;

            for (int x = 0; x < image.width; x++) {
                int fbX = dstX + x;
                if (fbX < 0 || fbX >= (int)fb->width)
                    continue;

                // source pixel
                int si = (y * image.width + x) * 4;
                uint8_t sR = image.data[si + 0];
                uint8_t sG = image.data[si + 1];
                uint8_t sB = image.data[si + 2];
                uint8_t sA = image.data[si + 3];

                // framebuffer pixel 위치 계산
                uint64_t pixelIndex = fbY * fb->pixelsPerScanLine + fbX;
                uint8_t* dstPx = fbBase + pixelIndex * 4;

                uint8_t dR = dstPx[2];
                uint8_t dG = dstPx[1];
                uint8_t dB = dstPx[0];

                // 알파 블렌딩
                uint8_t r = detail::blend8(sR, dR, sA);
                uint8_t g = detail::blend8(sG, dG, sA);
                uint8_t b = detail::blend8(sB, dB, sA);

                // BGR0 또는 BGRA32 프레임버퍼라고 가정
                dstPx[0] = b;
                dstPx[1] = g;
                dstPx[2] = r;
            }
        }
    }

} // namespace ribon::gfx