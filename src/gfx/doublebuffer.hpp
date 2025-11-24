#pragma once
#include <stdint.h>
#include <stddef.h>

namespace ribon::gfx {

    struct BackBufferInfo {
        uint32_t* base = nullptr;
        unsigned  width = 0;
        unsigned  height = 0;
        unsigned  pixelsPerScan = 0;
    };

    bool initDoubleBuffer();     // GOP FB 기반으로 백버퍼 생성
    void destroyDoubleBuffer();  // 메모리 해제

    bool isDoubleBufferEnabled();  

    // 현재 그릴 타겟 가져오기
    const BackBufferInfo* getDrawTarget();

    // 백버퍼 -> FB 로 복사
    void present();

} // namespace ribon::gfx