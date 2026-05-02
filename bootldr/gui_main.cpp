#include <Ribon/Common.hpp>
#include <Ribon/Console.hpp>
#include <Ribon/File.hpp>
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/InputSystem.hpp>
#include <Ribon/Print.hpp>
#include <Ribon/Screen.hpp>
#include <Ribon/Ui.hpp>

#include <Ribon/EfiContext.hpp>
#include <Ribon/boot/BootCore.hpp>
#include <Ribon/policy/AutoBootPolicy.hpp>
#include <Ribon/policy/GuiBootPolicy.hpp>

namespace {

void GuiStatusReporter(const char* message) {
    ribon::ui::showMessageLabel(100, 540, message);
    ribon::IO::Print<ribon::IO::Tags::DEBUG>(message);
}

void CliStatusReporter(const char* message) {
    ribon::IO::Print<ribon::IO::Tags::DEBUG>(message);
    ribon::IO::Print<ribon::IO::Tags::UTF16>("\r\n");
}

void BootKernelCallback(void*) {
    ribon::boot::ExecuteBootFlow(ribon::policy::DefaultGuiBootPolicy(), GuiStatusReporter);
}

void SettingsCallback(void*) {
    ribon::ui::showMessageLabel(100, 520, "Settings pressed!");
}

bool TestAutobootRequested() {
    ribon::str::Utf16String marker_path("\\ribon-test-autoboot");
    EFI_FILE_PROTOCOL* marker = ribon::IO::openFile(marker_path.c_str());
    if (!marker) {
        return false;
    }

    marker->Close(marker);
    return true;
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

    if (TestAutobootRequested()) {
        ribon::IO::Print<ribon::IO::Tags::DEBUG>(
            "GUI: test autoboot requested; entering autoboot.\r\n");
        ribon::boot::ExecuteBootFlow(
            ribon::policy::DefaultAutoBootPolicy(), CliStatusReporter);
        return EFI_SUCCESS;
    }

    /*
     * Graceful degradation: if the firmware did not give us a Graphics
     * Output Protocol (this is the common case on QEMU virt + AAVMF
     * builds without a virtio-gpu DXE driver bound), we cannot run the
     * GUI at all -- every drawing call would dereference a NULL gop
     * and abort with FAR=0x8 (gop->SetMode at offset 0x8 in the
     * EFI_GRAPHICS_OUTPUT_PROTOCOL vtable). Fall back to the autoboot
     * flow so the bootloader still makes forward progress.
     */
    if (ribon::getGop() == nullptr) {
        ribon::IO::Print<ribon::IO::Tags::DEBUG>(
            "GUI: no GOP available; falling back to autoboot.\r\n");
        ribon::boot::ExecuteBootFlow(
            ribon::policy::DefaultAutoBootPolicy(), CliStatusReporter);
        return EFI_SUCCESS;
    }

    ribon::CommonConfig& ccfg = ribon::GetCommonConfig();
    ccfg.renderMode = ribon::RenderMode::DoubleBuffer;
    ccfg.uiEnabled = true;
    ccfg.perfMode = true;

    console.SetMode(ribon::console::TextMode::FBFont);

    if (!ribon::gfx::initScreen(KAIRON_RIBON_GOP_WIDTH, KAIRON_RIBON_GOP_HEIGHT)) {
        ribon::IO::Print<ribon::IO::Tags::DEBUG>(
            "GUI: initScreen failed; falling back to autoboot.\r\n");
        ribon::boot::ExecuteBootFlow(
            ribon::policy::DefaultAutoBootPolicy(), CliStatusReporter);
        return EFI_SUCCESS;
    }
    ribon::gfx::clear(160, 165, 255, 255);

    ribon::IO::Print<ribon::IO::Tags::UTF16>("Ribon GUI Bootloader Starting...\r\n");

    using namespace ribon::ui;

    Panel* menu = createPanel(100, 100, 400, 300);
    menu->style.bg_r = 30;
    menu->style.bg_g = 30;
    menu->style.bg_b = 30;
    menu->style.bg_a = 255;
    menu->style.radius = 12;
    menu->style.titleBar = true;
    menu->style.titleText = "Boot Options";

    Label* label1 = createLabel(120, 150, "Ribon Bootloader UI");
    label1->r = 255; label1->g = 230; label1->b = 230; label1->a = 255;

    Button* btn1 = createButton(120, 200, 160, 40, "Boot Kairon");
    setButtonCallback(btn1, BootKernelCallback);
    btn1->r = 70; btn1->g = 120; btn1->b = 200; btn1->a = 255;

    Button* btn2 = createButton(120, 250, 160, 40, "Settings");
    setButtonCallback(btn2, SettingsCallback);
    btn2->r = 90; btn2->g = 90; btn2->b = 140; btn2->a = 255;

    registerRootWidget(menu);
    registerRootWidget(label1);
    registerRootWidget(btn1);
    registerRootWidget(btn2);

    UiLoopConfig cfg;
    cfg.bg_r = 160;
    cfg.bg_g = 165;
    cfg.bg_b = 255;
    cfg.bg_a = 255;
    cfg.targetFps = 60;

    runUiLoop(cfg);
    return EFI_SUCCESS;
}
