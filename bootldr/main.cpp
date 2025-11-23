#include <Ribon/Init.hpp>
#include <Ribon/String.hpp>
#include <Ribon/Print.hpp>
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/Screen.hpp>
#include <Ribon/Console.hpp>
#include <Ribon/File.hpp>
#include <Ribon/Image.hpp>

#include <Ribon/Ui.hpp>


#include <Ribon/Ui.hpp>

void TestUI() {
    {
        using namespace ribon;
        using namespace ribon::coord;
        using namespace ribon::coord::transform;
        using namespace ribon::ui;

        auto fb = ribon::fb::getFramebuffer();

        // ============================================
        // 1) 화면 절대 기준 박스 (Absolute)
        // ============================================
        AbsolutePos abs1 { 50, 50 };
        Rect r1 = toRect(abs1, 200, 80);    // 200x80 크기 박스

        drawRect(r1, 255, 0, 0);            // 빨간 박스


        // ============================================
        // 2) 화면 가운데 상대 박스 (Relative)
        // 부모 없음 → 수동으로 화면 전체 Rect 제공
        // ============================================
        Rect screen {
            0, 0,
            (int)fb->width,
            (int)fb->height
        };

        RelativePos rel1 {
            &screen,
            0.5f, 0.5f,   // 화면 중앙(anchor)
            -100, -40      // offset: 중앙에서 왼쪽/위로 이동
        };

        Rect r2 = toRect(rel1, 200, 80);

        drawRect(r2, 0, 255, 0);            // 초록 박스


        // ============================================
        // 3) 화면 오른쪽 아래에 붙는 박스
        // ============================================
        RelativePos rel2 {
            &screen,
            1.0f, 1.0f,   // 화면 오른쪽 아래(anchor)
            -150, -100    // 부모 오른쪽/아래로부터 오프셋
        };

        Rect r3 = toRect(rel2, 150, 100);

        drawRect(r3, 0, 0, 255);            // 파란 박스

        // 화면에 메시지 출력
        IO::Print("[UI Test] Render 3 boxes.\n");
    }
}

void TestImage() {
    {
        using namespace ribon;
        using namespace ribon::gfx;

        // 1) Root 디렉토리 열기
        EFI_FILE_PROTOCOL* root = nullptr;
        if (!ribon::IO::openRoot(&root)) {
            IO::Print("[ERR] Cannot open root FS\n");
            return;
        }

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

    ribon::console::Console console;
    ribon::console::setConsole(&console);

    console.SetMode(ribon::console::TextMode::FBFont);

    // 화면 정리
    auto st = ribon::getST();
    auto bs = ribon::getBS();


    ribon::gfx::initScreen(1200, 800);
    ribon::gfx::clear(255, 165, 160, 255);

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
    
    //TestImage();

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

    //Render(menu);

    // =====================================================================
    // Label - Panel 아래
    // =====================================================================
    Label* label1 = createLabel(120, 150, "Ribon Bootloader UI Test");
    label1->r = 255; label1->g = 230; label1->b = 230; label1->a = 255;

    Render(label1);

    // =====================================================================
    // Button 1
    // =====================================================================
    Button* btn1 = createButton(120, 200, 160, 40, "Start OS");
    btn1->r = 70; btn1->g = 120; btn1->b = 200; btn1->a = 255;
    Render(btn1);

    // =====================================================================
    // Button 2
    // =====================================================================
    Button* btn2 = createButton(120, 250, 160, 40, "Settings");
    btn2->r = 90; btn2->g = 90; btn2->b = 140; btn2->a = 255;

    Render(btn2);

    // =====================================================================
    // Layout 테스트 (개별 위젯으로 렌더)
    // =====================================================================
    Layout* layout = createLayout(550, 100, 300, 200, LayoutType::Vertical);
    Render(layout);

    Label* layoutLabel = createLabel(560, 110, "Layout Test Area");
    Render(layoutLabel);

    Button* layoutBtn1 = createButton(560, 150, 200, 40, "Item A");
    Render(layoutBtn1);

    Button* layoutBtn2 = createButton(560, 200, 200, 40, "Item B");
    Render(layoutBtn2);

    //TestPrintAllModes();



    
    // -----------------------------------
    //   5초 대기 (UEFI Stall 사용)
    // -----------------------------------
    // Stall() 단위: 1 마이크로초 (1 µs)
    // 5초 = 5,000,000 µs
    bs->Stall(20000000);


    return EFI_SUCCESS;
}
