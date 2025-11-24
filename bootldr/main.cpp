#include <Ribon/Init.hpp>
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


void TestUI() {
    using namespace ribon::ui;
    // Panel 생성
    Panel* menu = createPanel(100, 100, 400, 300);

    menu->style.bg_r = 30;
    menu->style.bg_g = 30;
    menu->style.bg_b = 30;
    menu->style.bg_a = 255;

    menu->style.radius = 12;
    menu->style.titleBar = true;
    menu->style.titleText = "Boot Options";

    renderSingleUi(menu);

    // =====================================================================
    // Label - Panel 아래
    // =====================================================================
    Label* label1 = createLabel(120, 150, "Ribon Bootloader UI Test");
    label1->r = 255; label1->g = 230; label1->b = 230; label1->a = 255;

    renderSingleUi(label1);

    // =====================================================================
    // Button 1
    // =====================================================================
    Button* btn1 = createButton(120, 200, 160, 40, "Start OS");
    btn1->r = 70; btn1->g = 120; btn1->b = 200; btn1->a = 255;
    renderSingleUi(btn1);

    // =====================================================================
    // Button 2
    // =====================================================================
    Button* btn2 = createButton(120, 250, 160, 40, "Settings");
    btn2->r = 90; btn2->g = 90; btn2->b = 140; btn2->a = 255;

    renderSingleUi(btn2);

    // =====================================================================
    // Layout 테스트 (개별 위젯으로 렌더)
    // =====================================================================
    Layout* layout = createLayout(550, 100, 300, 200, LayoutType::Vertical);
    renderSingleUi(layout);

    Label* layoutLabel = createLabel(560, 110, "Layout Test Area");
    renderSingleUi(layoutLabel);

    Button* layoutBtn1 = createButton(560, 150, 200, 40, "Item A");
    renderSingleUi(layoutBtn1);

    Button* layoutBtn2 = createButton(560, 200, 200, 40, "Item B");
    renderSingleUi(layoutBtn2);
}

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

void TestPrintAllModes()
{
    using namespace ribon;

    auto fb = fb::getFramebuffer();
    int centerX = fb->width / 2;
    int centerY = fb->height / 2;

    // ---------------------------------------------
    // 1) 기본 Print(fmt)
    // ---------------------------------------------
    IO::Print("Print Engine Test (Default Print : 1)\n");

    // ---------------------------------------------
    // 2) UTF16 모드
    // ---------------------------------------------
    IO::Print<IO::Tags::UTF16>(
        "Print Engine Test (UTF16 Mode : 2)\r\n"
    );

    // ---------------------------------------------
    // 3) RAW 모드
    // ---------------------------------------------
    IO::Print<IO::Tags::RAW>(
        "Print Engine Test (RAW Mode : 3)\n"
    );

    // ---------------------------------------------
    // 4) DEBUG 모드
    // ---------------------------------------------
    IO::Print<IO::Tags::DEBUG>(
        "Print Engine Test (DEBUG Mode : 4)\n"
    );

    // ---------------------------------------------
    // 5) FREE_RAW 모드 (XY 지정 출력)
    // 화면 정중앙에 출력
    // ---------------------------------------------
    {
        ribon::str::Utf16String u16("Print Engine Test (FREE_RAW Mode : 5)");

        // FREE_RAW는 좌표를 외부에서 지정하는 방식
        IO::FreePrintRawAt(centerX - 200, centerY, u16.c_str());
    }
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
    ccfg.perfMode   = true;                             // 성능 우선 모드 유지

    ribon::console::Console console;
    ribon::console::setConsole(&console);

    console.SetMode(ribon::console::TextMode::FBFont);

    // 화면 정리
    auto st = ribon::getST();
    auto bs = ribon::getBS();


    ribon::gfx::initScreen(1200, 800);
    ribon::gfx::clear(160, 165, 255, 255);

    // TestUI();
    

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

    using namespace ribon::ui;

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
    btn1->r = 70; btn1->g = 120; btn1->b = 200; btn1->a = 255;

    Button* btn2 = createButton(120, 250, 160, 40, "Settings");
    btn2->r = 90; btn2->g = 90; btn2->b = 140; btn2->a = 255;

    Layout* layout = createLayout(550, 100, 300, 200, LayoutType::Vertical);
    Label* layoutLabel = createLabel(560, 110, "Layout Test Area");
    Button* layoutBtn1 = createButton(560, 150, 200, 40, "Item A");
    Button* layoutBtn2 = createButton(560, 200, 200, 40, "Item B");

    // 루트 위젯 목록 (지금 구조에서는 개별 Render였으니, 그대로 나열)
    ribon::ui::Widget* roots[] = {
        menu,
        label1,
        btn1,
        btn2,
        layout,
        layoutLabel,
        layoutBtn1,
        layoutBtn2
    };

    ribon::ui::UiLoopConfig cfg;
    cfg.bg_r = 160;
    cfg.bg_g = 165;
    cfg.bg_b = 255;
    cfg.bg_a = 255;
    cfg.targetFps = 60;
    
    // TestImage();

    ribon::ui::runUiLoop(roots, sizeof(roots) / sizeof(roots[0]), cfg);

    
    
    // -----------------------------------
    //   5초 대기 (UEFI Stall 사용)
    // -----------------------------------
    // Stall() 단위: 1 마이크로초 (1 µs)
    // 5초 = 5,000,000 µs
    //bs->Stall(200000000);
    return EFI_SUCCESS;
}
