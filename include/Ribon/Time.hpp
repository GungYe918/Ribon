#pragma once
#include <stdint.h>
#include <Ribon/Init.hpp>


namespace ribon {

    /**
     * @brief 현재 시각을 마이크로초 단위로 반환
     *
     * - 기준(epoch)은 "오늘 0시부터 경과 시간" 정도로만 생각하면 된다.
     * - UEFI RuntimeServices->GetTime()을 사용하므로 부트로더 UI 용도에는 충분.
     */
    uint64_t nowMicroseconds() {
        auto st = ribon::getST();
        if (!st) return 0;

        EFI_TIME time{};
        EFI_STATUS status = st->RuntimeServices->GetTime(&time, nullptr);
        if (EFI_ERROR(status)) {
            return 0;
        }

        // 날짜(Y/M/D)는 무시하고, "하루 중 경과 시간"만 사용
        uint64_t seconds =
            (uint64_t)time.Hour   * 3600ull +
            (uint64_t)time.Minute * 60ull   +
            (uint64_t)time.Second;

        uint64_t usec = seconds * 1000000ull;

        // Nanosecond 필드를 마이크로초 단위로 환산 (있으면 사용, 없으면 0)
        usec += (uint64_t)(time.Nanosecond / 1000u);

        return usec;
    }

    /**
     * @brief 지정한 마이크로초 동안 슬립
     *
     * - 내부적으로 UEFI BootServices->Stall() 사용
     */
    void sleepMicroseconds(uint64_t usec) {
        auto bs = ribon::getBS();
        if (!bs || usec == 0) return;

        bs->Stall((UINTN)usec);
    }

} // namespace ribon