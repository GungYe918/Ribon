#include <Ribon/EfiContext.hpp>


extern "C" {
    int _fltused = 0;
    #include <Protocol/LoadedImage.h>
}

namespace ribon {

    static EFI_SYSTEM_TABLE*                gST             = nullptr;
    static EFI_BOOT_SERVICES*               gBS             = nullptr;
    static EFI_GRAPHICS_OUTPUT_PROTOCOL*    gGop            = nullptr;
    static EFI_HANDLE                       gImageHandle    = nullptr;
    static EFI_FILE_PROTOCOL*               gFileRoot           = nullptr;

    void initialize(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {
        gImageHandle = ImageHandle;
        gST = SystemTable;
        gBS = SystemTable->BootServices;


        EFI_STATUS status = gBS->LocateProtocol(
            &gEfiGraphicsOutputProtocolGuid,
            nullptr,
            (void**)&gGop
        );

        if (EFI_ERROR(status)) {
            // GOP를 얻지 못해도 텍스트 출력은 가능하므로
            // gGop = nullptr 그대로 둔다.
            gGop = nullptr;
        }

        // 파일시스템 root 얻기
        EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
        gBS->HandleProtocol(gImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&LoadedImage);

        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;
        gBS->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&FileSystem);

        FileSystem->OpenVolume(FileSystem, &gFileRoot);
    }   

    EFI_SYSTEM_TABLE*               getST()             {  return gST;              }
    EFI_BOOT_SERVICES*              getBS()             {  return gBS;              }
    EFI_GRAPHICS_OUTPUT_PROTOCOL*   getGop()            {  return gGop;             }
    EFI_HANDLE                      getImageHandle()    {  return gImageHandle;     }
    EFI_FILE_PROTOCOL*              getFileRoot()       {  return gFileRoot;        }

    bool selectGopMode(UINTN width, UINTN height) {
        if (!gBS || width == 0 || height == 0) {
            return false;
        }

        EFI_HANDLE* handles = nullptr;
        UINTN handleCount = 0;
        EFI_STATUS status = gBS->LocateHandleBuffer(
            ByProtocol,
            &gEfiGraphicsOutputProtocolGuid,
            nullptr,
            &handleCount,
            &handles
        );
        if (EFI_ERROR(status) || !handles) {
            return false;
        }

        bool selected = false;
        for (UINTN handleIndex = 0; handleIndex < handleCount && !selected; ++handleIndex) {
            EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = nullptr;
            status = gBS->HandleProtocol(
                handles[handleIndex],
                &gEfiGraphicsOutputProtocolGuid,
                (void**)&gop
            );
            if (EFI_ERROR(status) || !gop || !gop->Mode) {
                continue;
            }

            for (UINT32 mode = 0; mode < gop->Mode->MaxMode; ++mode) {
                EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = nullptr;
                UINTN infoSize = 0;
                if (gop->QueryMode(gop, mode, &infoSize, &info) != EFI_SUCCESS || !info) {
                    continue;
                }
                if (info->HorizontalResolution == width &&
                    info->VerticalResolution == height &&
                    gop->SetMode(gop, mode) == EFI_SUCCESS) {
                    gGop = gop;
                    selected = true;
                    break;
                }
            }
        }

        gBS->FreePool(handles);
        return selected;
    }

} // namespace ribon
