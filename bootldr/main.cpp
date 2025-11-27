#include <Ribon/EfiContext.hpp>
#include <Ribon/String.hpp>
#include <Ribon/Print.hpp>
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/Screen.hpp>
#include <Ribon/Console.hpp>
#include <Ribon/File.hpp>
#include <Ribon/Image.hpp>

#include <Ribon/InputSystem.hpp>

#include <loaderPkg/LBPBLoader.hpp>
#include <loaderPkg/MultibootLoader.hpp>
#include <loaderPkg/BootLogic.hpp>
#include <loaderPkg/FiascoLoader.hpp>


#include <Ribon/Ui.hpp>
#include <Ribon/Common.hpp>


void BootKernelCallback(void*) {
    using namespace ribon;
    using namespace ribon::loaderPkg::boot;
    using namespace ribon::loaderPkg;
    using namespace ribon::IO;

    // 화면 상태 메시지 출력
    ui::showMessageLabel(100, 500, "Booting kernel...");

    //
    // 1) ESP 루트에서 커널 파일 읽기
    //
    EFI_FILE_PROTOCOL* root = ribon::getFileRoot();
    if (!root) {
        ui::showMessageLabel(100, 530, "ERR: No filesystem root.");
        return;
    }

    // 일단 테스트용 이름: \kernel.elf
    str::Utf16String kernelPath("\\kernel\\kernel.elf");

    EFI_FILE_PROTOCOL* kfile = IO::openFile(kernelPath.c_str(), EFI_FILE_MODE_READ);
    if (!kfile) {
        ui::showMessageLabel(100, 530, "ERR: kernel.elf not found");
        return;
    }

    UINT64 ksize = IO::fileSize(kfile);
    if (ksize == 0) {
        ui::showMessageLabel(100, 530, "ERR: Kernel size = 0");
        return;
    }

    void* kbuf = nullptr;
    if (EFI_ERROR(mem::AllocatePool(EfiLoaderData, ksize, &kbuf))) {
        ui::showMessageLabel(100, 530, "ERR: No memory for kernel");
        return;
    }

    UINTN readSize = 0;
    if (!IO::readFile(kfile, kbuf, (UINTN)ksize, &readSize)) {
        ui::showMessageLabel(100, 530, "ERR: Kernel read fail");
        return;
    }

    kfile->Close(kfile);

    //
    // 2) BootLogic + Loader 등록
    //
    static BootLogic        bootLogic;
    static MultibootLoader  mbLoader;
    static FiascoLoader     fiasLoader;
    static LBPBLoader       lbpbLoader;

    // 등록 순서에 따라 우선순위 결정
    bootLogic.registerLoader(&lbpbLoader);
    //bootLogic.registerLoader(&fiasLoader);
    //bootLogic.registerLoader(&mbLoader);
    bootLogic.setKernel(kbuf, (UINTN)ksize);

    //
    // 3) probe + load
    //
    if (!bootLogic.boot()) {
        ui::showMessageLabel(100, 530, "ERR: Boot failed (no matching loader)");
        return;
    }

    ui::showMessageLabel(100, 540, "Kernel loaded. ExitBootServices...");

    ribon::ui::requestUiEndLoop();

    //
    // 4) 그래픽/UI 종료 -> ExitBootServices
    //
    if (!bootLogic.exitBootServices()) {
        // ExitBootServices 실패시 그래픽 출력 불가 -> fallback 없음
        return;
    }

    // 5) 이제 실제 커널로 점프 (반환 없음)
    bootLogic.jumpToKernel();

    // 여기까지 올 일 없음
}


void SettingsCallback(void*) {
    ribon::ui::showMessageLabel(100, 520, "Settings pressed!");
}

extern "C"
EFI_STATUS
EFIAPI
EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
    // Ribon 초기화
    ribon::initialize(ImageHandle, SystemTable);

    ribon::CommonConfig& ccfg = ribon::GetCommonConfig();
    ccfg.renderMode = ribon::RenderMode::DoubleBuffer;  // 더블 버퍼링 활성화
    ccfg.uiEnabled  = true;                             // DirtyLevel 설정
    ccfg.perfMode   = true;                             // 성능 우선 모드 유지

    ribon::console::Console console;
    ribon::console::setConsole(&console);

    console.SetMode(ribon::console::TextMode::FBFont);

    // 화면 정리
    auto st = ribon::getST();
    auto bs = ribon::getBS();

    ribon::gfx::initScreen(1200, 800);
    ribon::gfx::clear(160, 165, 255, 255);

    // 배너 출력 (처음 1프레임에만 보이고 이후 UI 루프에서 덮일 수 있음)
    ribon::IO::Print<ribon::IO::Tags::UTF16>(
        "Ribon EFI Bootloader Starting...\r\n"
    );
    ribon::IO::Print<ribon::IO::Tags::UTF16>(
        "Hello? %d\n", 42
    );
    ribon::IO::Print<ribon::IO::Tags::UTF16>(
        "GOP loaded successfully.\r\n"
    );

    using namespace ribon::ui;

    // ---------------------------
    //  UI 위젯 구성
    // ---------------------------
    Panel* menu = createPanel(100, 100, 400, 300);
    menu->style.bg_r = 30;
    menu->style.bg_g = 30;
    menu->style.bg_b = 30;
    menu->style.bg_a = 255;
    menu->style.radius   = 12;
    menu->style.titleBar = true;
    menu->style.titleText = "Boot Options";

    Label* label1 = createLabel(120, 150, "Ribon Bootloader UI Test");
    label1->r = 255; label1->g = 230; label1->b = 230; label1->a = 255;

    Button* btn1 = createButton(120, 200, 160, 40, "CreateButton");
    setButtonCallback(btn1, BootKernelCallback);
    btn1->r = 70; btn1->g = 120; btn1->b = 200; btn1->a = 255;

    Button* btn2 = createButton(120, 250, 160, 40, "Settings");
    setButtonCallback(btn2, SettingsCallback);
    btn2->r = 90; btn2->g = 90; btn2->b = 140; btn2->a = 255;

    Layout* layout = createLayout(550, 100, 300, 200, LayoutType::Vertical);
    Label* layoutLabel = createLabel(560, 110, "Layout Test Area");
    Button* layoutBtn1 = createButton(560, 150, 200, 40, "Item A");
    Button* layoutBtn2 = createButton(560, 200, 200, 40, "Item B");

    // 필요하면 IFrame도 여기서 생성 후 등록 가능
    // ribon::str::Utf16String path16("\\test.png");
    // IFrame* imgFrame = createIFrame(10, 10, 200, 200, path16.c_str());

    // ---------------------------
    //  루트 위젯 동적 등록
    // ---------------------------
    registerRootWidget(menu);
    registerRootWidget(label1);
    registerRootWidget(btn1);
    registerRootWidget(btn2);
    registerRootWidget(layout);
    registerRootWidget(layoutLabel);
    registerRootWidget(layoutBtn1);
    registerRootWidget(layoutBtn2);
    // registerRootWidget(imgFrame);

    // ---------------------------
    //  UI 루프 실행
    // ---------------------------
    UiLoopConfig cfg;
    cfg.bg_r = 160;
    cfg.bg_g = 165;
    cfg.bg_b = 255;
    cfg.bg_a = 255;
    cfg.targetFps = 60;

    ribon::IO::Print<ribon::IO::Tags::UTF16>(
        "GOP loaded successfully.\r\n"
    );

    runUiLoop(cfg);

    // 여기까지 오지 않음 (무한 루프) 이지만, 형식상 반환
    return EFI_SUCCESS;
}
