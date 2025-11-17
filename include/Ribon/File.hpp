#pragma once

extern "C" {
    #include <Uefi.h>
    #include <Protocol/SimpleFileSystem.h>
    #include <Protocol/LoadedImage.h>
}

#include <Ribon/Init.hpp>

//
// UEFI spec EFI_FILE_INFO structure
//
#pragma pack(push, 1)
typedef struct {
    UINT64   Size;
    UINT64   FileSize;
    UINT64   PhysicalSize;
    EFI_TIME CreateTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    UINT64   Attribute;
    CHAR16   FileName[1];
} EFI_FILE_INFO;
#pragma pack(pop)


//
// GUID: gEfiFileInfoGuid
//

static EFI_GUID gEfiFileInfoGuid = {
    0x09576e92, 0x6d3f, 0x11d2,
    { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b }
};

namespace ribon::IO {

    inline bool openRoot(EFI_FILE_PROTOCOL** root) {
        EFI_LOADED_IMAGE_PROTOCOL* loadedImage = nullptr;

        // 1) 현재 이미지에 대한 LOADED_IMAGE 얻기
        EFI_STATUS st = ribon::getBS()->HandleProtocol(
            ribon::getImageHandle(),
            &gEfiLoadedImageProtocolGuid,
            (void**)&loadedImage
        );
        if (EFI_ERROR(st)) return false;

        // 2) FS는 LoadedImage->DeviceHandle 에 있다
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs = nullptr;
        st = ribon::getBS()->HandleProtocol(
            loadedImage->DeviceHandle,
            &gEfiSimpleFileSystemProtocolGuid,
            (void**)&fs
        );
        if (EFI_ERROR(st)) return false;

        // 3) Root volume 열기
        st = fs->OpenVolume(fs, root);
        return !EFI_ERROR(st);
    }


} // namespace ribon::IO

//
// Attributes
//
#define EFI_FILE_READ_ONLY           0x00000001
#define EFI_FILE_HIDDEN              0x00000002
#define EFI_FILE_SYSTEM              0x00000004
#define EFI_FILE_RESERVED            0x00000008
#define EFI_FILE_DIRECTORY           0x00000010
#define EFI_FILE_ARCHIVE             0x00000020
#define EFI_FILE_VALID_ATTR          0x00000037