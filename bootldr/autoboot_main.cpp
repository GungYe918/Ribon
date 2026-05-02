#include <Ribon/Console.hpp>
#include <Ribon/EfiContext.hpp>
#include <Ribon/Print.hpp>

#include <Ribon/boot/BootCore.hpp>
#include <Ribon/policy/AutoBootPolicy.hpp>

namespace {

void AutoBootReporter(const char* message) {
    ribon::IO::Print<ribon::IO::Tags::DEBUG>(message);
    ribon::IO::Print<ribon::IO::Tags::UTF16>("\r\n");
}

void TrySetGopMode(UINTN width, UINTN height) {
    if (width == 0 || height == 0) {
        return;
    }

    if (ribon::selectGopMode(width, height)) {
        ribon::IO::Print<ribon::IO::Tags::DEBUG>(
            "Autoboot GOP mode set.\r\n");
        return;
    }

    ribon::IO::Print<ribon::IO::Tags::DEBUG>(
        "Autoboot requested GOP mode unavailable; keeping firmware mode.\r\n");
}

} // namespace

extern "C"
EFI_STATUS
EFIAPI
EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {
    ribon::initialize(ImageHandle, SystemTable);

    ribon::console::Console console;
    ribon::console::setConsole(&console);
    console.SetMode(ribon::console::TextMode::SimpleText);

    TrySetGopMode(KAIRON_RIBON_GOP_WIDTH, KAIRON_RIBON_GOP_HEIGHT);

    ribon::IO::Print<ribon::IO::Tags::UTF16>("Ribon autoboot starting...\r\n");
    if (!ribon::boot::ExecuteBootFlow(ribon::policy::DefaultAutoBootPolicy(), AutoBootReporter)) {
        ribon::IO::Print<ribon::IO::Tags::UTF16>("Autoboot failed.\r\n");
        return EFI_LOAD_ERROR;
    }

    return EFI_SUCCESS;
}
