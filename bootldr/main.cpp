#include <Ribon/EfiContext.hpp>
#include <Ribon/String.hpp>
#include <Ribon/Print.hpp>
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/Screen.hpp>
#include <Ribon/Console.hpp>
#include <Ribon/File.hpp>
#include <Ribon/Image.hpp>

#include <Ribon/InputSystem.hpp>

#include <Ribon/Ui.hpp>
#include <Ribon/Common.hpp>

void TestImage() {
    {
        using namespace ribon;
        using namespace ribon::gfx;

        // 1) Root 디렉토리 열기
        EFI_FILE_PROTOCOL* root = getFileRoot();

        // 2) PNG 로드 (ESP/logo.png)
        ribon::str::Utf16String path16("\\test.png");
        Image img = Image::loadFromFile(root, path16.c_str());

        if (!img.data) {
            IO::Print("[ERR] Image load fail\n");
            return;
        }

        IO::Print("[INFO] PNG loaded: %dx%d\n", img.width, img.height);

        // 3) 화면 중앙에 이미지 렌더링
        auto fb = ribon::fb::getFramebuffer();
        int cx = (int)(fb->width / 2 - img.width / 2);
        int cy = (int)(fb->height / 2 - img.height / 2);

        drawImage(cx, cy, img);

        // 4) 나중에 해제
        img.unload();
    }
}

void StartOsCallback(void*) {
    ribon::ui::showMessageLabel(100, 500, "Start OS pressed!");
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

    Button* btn1 = createButton(120, 200, 160, 40, "Start OS");
    setButtonCallback(btn1, StartOsCallback);
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
