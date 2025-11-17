#pragma once

extern "C" {
    #include <Uefi.h>
    #include <Protocol/GraphicsOutput.h>
}

namespace ribon::fb {

    struct FrameBufferInfo {
        UINT32* base;       // framebuffer 물리 주소
        UINTN   width;
        UINTN   height;
        UINTN   pixelsPerScanLine;
        UINTN   sizeInBytes;
    };

    // 프레임버퍼 초기화 (gGop 사용)
    bool initFrameBuffer();

    // Framebuffer 정보 가져오기
    const FrameBufferInfo* getFramebuffer();

    /** @brief 단일 픽셀 RAW 쓰기
     *  경계 검사 없음(최대한 빠르게 수행)
     */
    void writePixelRaw(UINTN x, UINTN y, UINT32 rgba);

    /** @brief 안전한 픽셀 쓰기(범위 체크 포함) */
    void writePixelSafe(UINTN x, UINTN y, UINT32 rgba);

    
    /**
     * Utils
     */
    

    /** @brief 전체 화면 Clear (RGB 8비트) */
    void clear(UINT8 r, UINT8 g, UINT8 b);

    /** @brief 프레임버퍼 dump (32비트 단위 메모리 출력) */
    void debugDump(UINTN count = 64);

    /** @brief 현재 모드 정보 출력 */
    void debugPrintInfo();

} // namespace ribon::fb