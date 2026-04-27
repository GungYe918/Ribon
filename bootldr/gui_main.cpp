#include <Ribon/Common.hpp>
#include <Ribon/Console.hpp>
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/InputSystem.hpp>
#include <Ribon/Print.hpp>
#include <Ribon/Screen.hpp>
#include <Ribon/Ui.hpp>

#include <Ribon/EfiContext.hpp>
#include <Ribon/boot/BootCore.hpp>
#include <Ribon/policy/GuiBootPolicy.hpp>

namespace {

void GuiStatusReporter(const char* message) {
    ribon::ui::showMessageLabel(100, 540, message);
    ribon::IO::Print<ribon::IO::Tags::DEBUG>(message);
}

void BootKernelCallback(void*) {
    ribon::boot::ExecuteBootFlow(ribon::policy::DefaultGuiBootPolicy(), GuiStatusReporter);
}

void SettingsCallback(void*) {
    ribon::ui::showMessageLabel(100, 520, "Settings pressed!");
}

} // namespace

extern "C"
EFI_STATUS
EFIAPI
EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {
    ribon::initialize(ImageHandle, SystemTable);

    ribon::CommonConfig& ccfg = ribon::GetCommonConfig();
    ccfg.renderMode = ribon::RenderMode::DoubleBuffer;
    ccfg.uiEnabled = true;
    ccfg.perfMode = true;

    ribon::console::Console console;
    ribon::console::setConsole(&console);
    console.SetMode(ribon::console::TextMode::FBFont);

    ribon::gfx::initScreen(1200, 800);
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
