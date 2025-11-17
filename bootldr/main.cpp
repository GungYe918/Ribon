extern "C" {
    #include <Uefi.h>
    #include <Protocol/SimpleTextOut.h>
    #include <Protocol/SimpleTextIn.h>
    #include <Protocol/GraphicsOutput.h>
}

#include <Ribon/Init.hpp>
#include <Ribon/base/Utf16String.hpp>
#include <Ribon/Print.hpp>

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
    ribon::IO::Print<ribon::IO::Tags::RAW>(banner);

    ribon::IO::Print<ribon::IO::Tags::UTF16>("Hello? %d\n", 10);


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

    
    // -----------------------------------
    //   5초 대기 (UEFI Stall 사용)
    // -----------------------------------
    // Stall() 단위: 1 마이크로초 (1 µs)
    // 5초 = 5,000,000 µs
    bs->Stall(5000000);

    return EFI_SUCCESS;

    return EFI_SUCCESS;
}
