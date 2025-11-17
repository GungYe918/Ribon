#include <Ribon/Init.hpp>
#include <Ribon/String.hpp>
#include <Ribon/Print.hpp>
#include <Ribon/FrameBuffer.hpp>
#include <Ribon/Screen.hpp>
#include <Ribon/Console.hpp>
#include <Ribon/File.hpp>
#include <Ribon/Image.hpp>

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


    ribon::gfx::initScreen(1920, 1080);
    ribon::gfx::clear(255, 165, 160, 255);

    TestUI();
    

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
    
    TestImage();
    
    // -----------------------------------
    //   5초 대기 (UEFI Stall 사용)
    // -----------------------------------
    // Stall() 단위: 1 마이크로초 (1 µs)
    // 5초 = 5,000,000 µs
    bs->Stall(20000000);


    return EFI_SUCCESS;
}
