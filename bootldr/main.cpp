extern "C" {
    #include <Uefi.h>
    #include <Protocol/SimpleTextOut.h>
    #include <Protocol/SimpleTextIn.h>
    #include <Protocol/GraphicsOutput.h>
}

#include <Ribon/Init.hpp>
#include <Ribon/base/Utf16String.hpp>
#include <Ribon/Print.hpp>
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/Screen.hpp>
#include <Ribon/Console.hpp>


extern "C"
EFI_STATUS
EFIAPI
EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
    // Ribon 초기화
    ribon::initialize(SystemTable);

    ribon::console::Console console;
    ribon::console::setConsole(&console);

    console.SetMode(ribon::console::TextMode::FBFont);

    // 화면 정리
    auto st = ribon::getST();
    auto bs = ribon::getBS();


    ribon::gfx::initScreen(800, 600);
    ribon::gfx::clear(221, 160, 255, 1);

    


    // 배너 출력
    ribon::IO::Print<ribon::IO::Tags::UTF16>(
        "Ribon EFI Bootloader Starting...\r\n"
    );

    ribon::IO::Print<ribon::IO::Tags::UTF16>(
        "Hello? %d\n", 42
    );

    ribon::IO::Print<ribon::IO::Tags::UTF16>(
        "GOP loaded successfully.\r\n"
    );

    
    // -----------------------------------
    //   5초 대기 (UEFI Stall 사용)
    // -----------------------------------
    // Stall() 단위: 1 마이크로초 (1 µs)
    // 5초 = 5,000,000 µs
    bs->Stall(5000000);

    return EFI_SUCCESS;

    return EFI_SUCCESS;
}
