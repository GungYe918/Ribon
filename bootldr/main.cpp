extern "C" {
    #include <Uefi.h>
    #include <Protocol/SimpleTextOut.h>
    #include <Protocol/SimpleTextIn.h>
    #include <Protocol/GraphicsOutput.h>
}

#include <Ribon/Init.hpp>
#include <Ribon/Utf16String.hpp>

EFI_GRAPHICS_OUTPUT_PROTOCOL *gGop = nullptr;

extern "C"
EFI_STATUS
EFIAPI
EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
    // Ribon 초기화
    ribon::initialize(SystemTable);

    // 화면 정리
    auto st = ribon::getST();
    st->ConOut->ClearScreen(st->ConOut);

    // Utf16String
    ribon::str::Utf16String banner("Ribon EFI Bootloader Starting...\r\n");

    // Gop 로딩
    auto bs = ribon::getBS();

    EFI_STATUS status = bs->LocateProtocol(
        &gEfiGraphicsOutputProtocolGuid,
        nullptr,
        (void**)&gGop
    );

    if (EFI_ERROR(status)) {
        ribon::str::Utf16String err("GOP protocol not found.\r\n");
        st->ConOut->OutputString(st->ConOut, err.c_str());
        return status;
    }

    ribon::str::Utf16String ok("GOP loaded successfully.\r\n");
    st->ConOut->OutputString(st->ConOut, ok.c_str());

    return EFI_SUCCESS;
}
