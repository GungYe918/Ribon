#pragma once
#include <Uefi.h>

namespace ribon::str {

    /**
     * @brief UTF-8 문자열 클래스 (header-only, lightweight)
     * 
     * - 변환 비용 최소화를 위해 실제로는 외부 버퍼를 소유하지 않음
     * - UTF-16 입력 시 단순 ANSI 캐스팅 기반 변환 수행
     * - UTF-16 → UTF-8 변환이 필요해질 경우 확장 가능
     */
    class Utf8String {
    public:
        /** @brief UTF-8 (char*) 생성자 */
        inline Utf8String(const char* utf8)
            : buf(utf8), len(calc_len(utf8)) {}

        /** @brief UTF-16 (CHAR16*) 생성자 — ASCII 기반 캐스팅 */
        inline Utf8String(const CHAR16* utf16) {
            if (!utf16) {
                buf = empty;
                len = 0;
                return;
            }

            // UTF-16 길이 파악
            UINTN wlen = 0;
            while (utf16[wlen] != 0) wlen++;

            // 내부 static 버퍼에 변환 (UTF-8과 바이너리 호환 범위만 지원)
            // NOTE: 전체 UTF-8 변환이 필요해지면 확장 가능한 구조
            if (wlen >= INTERNAL_MAX)  wlen = INTERNAL_MAX - 1;

            for (UINTN i = 0; i < wlen; i++)
                internal[i] = (char)(utf16[i] & 0xFF);

            internal[wlen] = 0;

            buf = internal;
            len = wlen;
        }

        /** @brief 복사 생성 불가 (가벼운 핸들 개념) */
        Utf8String(const Utf8String&) = delete;
        Utf8String& operator=(const Utf8String&) = delete;

        /** @brief 이동 생성자 */
        inline Utf8String(Utf8String&& other) noexcept
            : buf(other.buf), len(other.len) {
            other.buf = empty;
            other.len = 0;
        }

        /** @brief 이동 대입 */
        inline Utf8String& operator=(Utf8String&& other) noexcept {
            if (this != &other) {
                buf = other.buf;
                len = other.len;

                other.buf = empty;
                other.len = 0;
            }
            return *this;
        }

        /** @brief 문자열 포인터 반환 */
        inline const char* c_str() const { return buf; }

        /** @brief 문자열 길이 반환 */
        inline UINTN length() const { return len; }

    private:
        const char* buf = empty;
        UINTN       len = 0;

        // 내부 변환용 버퍼
        static constexpr UINTN INTERNAL_MAX = 256;
        char internal[INTERNAL_MAX] = {0};

        static inline char empty[1] = {0};

        /** @brief char* 길이 계산 */
        inline static UINTN calc_len(const char* s) {
            if (!s) return 0;
            UINTN n = 0;
            while (s[n]) n++;
            return n;
        }
    };

} // namespace ribon::str
