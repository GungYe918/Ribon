#pragma once

#include <Uefi.h>
#include "LoaderBase.hpp"
#include "Multiboot.hpp"

#include <Ribon/Elf.hpp>
#include <Ribon/EfiContext.hpp>

namespace ribon::loaderPkg {

    /**
     * @brief Multiboot v1 / v2 자동 판별 로더
     *
     * - LoaderBase<MultibootLoader> 사용
     * - virtual ZERO
     * - ELF 기반 커널 로딩 지원
     * - 헤더 온리
     */
    class MultibootLoader : public LoaderBase<MultibootLoader> {
    public:
        constexpr MultibootLoader() = default;

        /**
         * @brief 멀티부트 헤더 검출
         * 파일의 앞부분 1KB 범위 내에서 magic을 찾음.
         */
        constexpr bool probe(const void* file, UINTN size) const {
            if (size < 32) return false;
            const UINT8* p = static_cast<const UINT8*>(file);

            // v1 헤더 스캔 (GRUB는 보통 처음 8192 bytes 검사)
            for (UINTN i = 0; i + sizeof(MultibootHeader) <= size && i < 8192; i += 4) {
                auto hdr = reinterpret_cast<const MultibootHeader*>(p + i);
                if (hdr->magic == MULTIBOOT_HEADER_MAGIC) {
                    // 체크섬 확인
                    UINT32 sum = hdr->magic + hdr->flags + hdr->checksum;
                    if (sum == 0)
                        return true;
                }
            }

            // v2 헤더 스캔
            for (UINTN i = 0; i + sizeof(Multiboot2Header) <= size && i < 8192; i += 8) {
                auto hdr = reinterpret_cast<const Multiboot2Header*>(p + i);
                if (hdr->magic == MULTIBOOT2_HEADER_MAGIC)
                    return true;
            }

            return false;
        }

        bool load(const void* file, UINTN size) {
            if (size < sizeof(Elf64_Ehdr)) return false;

            // ELF 헤더 확인
            const auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(file);
            if (!IS_ELF(*ehdr) || !IS_ELF64(*ehdr)) return false;

            // 엔트리 기록
            entry_ = ehdr->e_entry;

            // 프로그램 헤더 기반 로드
            const UINT8* base = static_cast<const UINT8*>(file);
            const auto* phdr = reinterpret_cast<const Elf64_Phdr*>(base + ehdr->e_phoff);

            for (UINTN i = 0; i < ehdr->e_phnum; i++) {
                if (phdr[i].p_type == PT_LOAD) {
                    UINT64 dst     = phdr[i].p_vaddr;
                    UINT64 src_off = phdr[i].p_offset;

                    // 파일 데이터를 메모리로 복사
                    getBS()->CopyMem(
                        reinterpret_cast<void*>(dst),
                        const_cast<UINT8*>(base + src_off),
                        static_cast<UINTN>(phdr[i].p_filesz)
                    );

                    // BSS clear
                    UINT64 bss = dst + phdr[i].p_filesz;
                    UINT64 bss_size = phdr[i].p_memsz - phdr[i].p_filesz;
                    if (bss_size > 0) {
                        getBS()->SetMem(
                            reinterpret_cast<void*>(bss),
                            static_cast<UINTN>(bss_size),
                            0
                        );
                    }
                }
            }

            return true;
        }


        /**
         * @brief 로드 완료 후 엔트리 주소 반환
         */
        constexpr UINT64 entryPoint() const {
            return entry_;
        }

    private:
        UINT64 entry_ = 0;
    };

} // namespace ribon::loaderPkg