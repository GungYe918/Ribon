#pragma once

extern "C" {
    #include <Uefi.h>
    #include <Protocol/SimpleFileSystem.h>
    #include <Protocol/LoadedImage.h>
}

#include <Ribon/EfiContext.hpp>
#include <Memory.hpp>

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

    inline EFI_FILE_PROTOCOL* getFileRoot() {
        return ribon::getFileRoot();
    }

    // -------------------------------
    // File Open
    // -------------------------------
    inline EFI_FILE_PROTOCOL* openFile(const CHAR16* path, UINT64 mode = EFI_FILE_MODE_READ) {
        EFI_FILE_PROTOCOL* root = getFileRoot();
        if (!root) return nullptr;

        EFI_FILE_PROTOCOL* file = nullptr;
        EFI_STATUS st = root->Open(
            root,
            &file,
            (CHAR16*)path,
            mode,
            0
        );

        return EFI_ERROR(st) ? nullptr : file;
    }

    // -------------------------------
    // File Read - full read into buffer
    // -------------------------------
    inline bool readFile(EFI_FILE_PROTOCOL* file, void* buffer, UINTN bufferSize, UINTN* readSize = nullptr)
    {
        if (!file || !buffer) return false;

        UINTN sz = bufferSize;
        EFI_STATUS st = file->Read(file, &sz, buffer);

        if (EFI_ERROR(st)) return false;
        if (readSize) *readSize = sz;
        return true;
    }

    inline UINT64 fileSize(EFI_FILE_PROTOCOL* file) {
        if (!file) return 0;

        UINTN infoSize = 0;
        file->GetInfo(file, &gEfiFileInfoGuid, &infoSize, nullptr);

        EFI_FILE_INFO* info = nullptr;
        if (EFI_ERROR(ribon::mem::AllocatePool(EfiLoaderData, infoSize, (void**)&info)))
            return 0;

        if (EFI_ERROR(file->GetInfo(file, &gEfiFileInfoGuid, &infoSize, info))) {
            ribon::mem::FreePool(info);
            return 0;
        }

        UINT64 size = info->FileSize;
        ribon::mem::FreePool(info);

        return size;
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