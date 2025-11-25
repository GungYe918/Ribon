#pragma once

#include <Uefi.h>
#include "LoaderBase.hpp"
#include "Multiboot.hpp"

#include <Ribon/Elf.hpp>
#include <Ribon/EfiContext.hpp>
#include <Ribon/Memory.hpp>

namespace ribon::loaderPkg {

    /**
     * @brief Multiboot v1 / v2 자동 로더
     *
     * - virtual ZERO
     * - ELF64 기반 커널 로딩 지원
     */
    class MultibootLoader : public LoaderBase<MultibootLoader> {
    public:
        constexpr MultibootLoader() = default;

        /**
         * @brief 멀티부트 헤더 검색
         * 파일의 앞부분 최대 8192 bytes 범위에서 magic 값 찾기
         */
        constexpr bool probe(const void* file, UINTN size) const {
            if (size < 32) return false;
            const UINT8* p = static_cast<const UINT8*>(file);

            // v1 헤더 스캔 (GRUB는 보통 처음 8192 bytes 검사)
            for (UINTN i = 0;
                 i + sizeof(MultibootHeader) <= size && i < 8192;
                 i += 4)
            {
                auto hdr = reinterpret_cast<const MultibootHeader*>(p + i);
                if (hdr->magic == MULTIBOOT_HEADER_MAGIC) {
                    // 체크섬 확인
                    UINT32 sum = hdr->magic + hdr->flags + hdr->checksum;
                    if (sum == 0)
                        return true;
                }
            }

            // v2 헤더 스캔
            for (UINTN i = 0;
                 i + sizeof(Multiboot2Header) <= size && i < 8192;
                 i += 8)
            {
                auto hdr = reinterpret_cast<const Multiboot2Header*>(p + i);
                if (hdr->magic == MULTIBOOT2_HEADER_MAGIC)
                    return true;
            }

            return false;
        }

        /**
         * @brief ELF64 커널 로드
         *
         * - ELF 매직 및 클래스 검사
         * - PT_LOAD 세그먼트를 vaddr로 복사
         * - BSS 영역은 0으로 초기화
         */
        bool load(const void* file, UINTN size) {
            if (size < sizeof(Elf64_Ehdr)) return false;

            const auto* eh = reinterpret_cast<const Elf64_Ehdr*>(file);
            if (!IS_ELF(*eh) || !IS_ELF64(*eh))
                return false;

            entry_ = eh->e_entry;

            const UINT8* base = static_cast<const UINT8*>(file);
            const auto* ph   = reinterpret_cast<const Elf64_Phdr*>(base + eh->e_phoff);

            for (UINTN i = 0; i < eh->e_phnum; ++i) {
                if (ph[i].p_type != PT_LOAD) continue;

                UINT8*       dst     = reinterpret_cast<UINT8*>(ph[i].p_vaddr);
                const UINT8* src     = base + ph[i].p_offset;
                UINTN        filesz  = static_cast<UINTN>(ph[i].p_filesz);
                UINTN        memsz   = static_cast<UINTN>(ph[i].p_memsz);

                // 파일 영역 복사
                ribon::mem::Memcpy(dst, src, filesz);

                // BSS 영역 0으로 초기화
                if (memsz > filesz) {
                    UINTN bss_size = memsz - filesz;
                    ribon::mem::Memset(dst + filesz, 0, bss_size);
                }
            }

            return true;
        }

        /**
         * @brief ExitBootServices 호출
         *
         * - UEFI 규약에 맞게 MemoryMap 두 번 시도
         * - 성공 시 BootServices는 완전히 종료됨
         */
        bool exitBootServices() {
            auto* bs = ribon::getBS();
            auto  ih = ribon::getImageHandle();

            if (!bs || !ih) return false;

            UINTN  mapSize   = 0;
            UINTN  mapKey    = 0;
            UINTN  descSize  = 0;
            UINT32 descVer   = 0;
            EFI_STATUS st;

            // 1) mapSize 얻기 (버퍼 크기 질의)
            st = bs->GetMemoryMap(&mapSize, nullptr, &mapKey, &descSize, &descVer);
            if (st != EFI_BUFFER_TOO_SMALL)
                return false;

            // 2) 버퍼 할당
            EFI_MEMORY_DESCRIPTOR* memMap = nullptr;
            st = ribon::mem::AllocatePool(EfiLoaderData, mapSize, (void**)&memMap);
            if (EFI_ERROR(st) || !memMap) return false;

            // 3) 실제 메모리맵 얻기
            st = bs->GetMemoryMap(&mapSize, memMap, &mapKey, &descSize, &descVer);
            if (EFI_ERROR(st)) {
                ribon::mem::FreePool(memMap);
                return false;
            }

            // 4) ExitBootServices 호출
            st = bs->ExitBootServices(ih, mapKey);

            if (EFI_ERROR(st)) {
                // 실패 -> 다시 시도
                ribon::mem::FreePool(memMap);

                mapSize = 0;
                st = bs->GetMemoryMap(&mapSize, nullptr, &mapKey, &descSize, &descVer);
                if (st != EFI_BUFFER_TOO_SMALL) return false;

                st = ribon::mem::AllocatePool(EfiLoaderData, mapSize, (void**)&memMap);
                if (EFI_ERROR(st) || !memMap) return false;

                st = bs->GetMemoryMap(&mapSize, memMap, &mapKey, &descSize, &descVer);
                if (EFI_ERROR(st)) {
                    ribon::mem::FreePool(memMap);
                    return false;
                }

                st = bs->ExitBootServices(ih, mapKey);
                if (EFI_ERROR(st)) {
                    // 여기서는 더 이상 BootServices에 의존할 수 없다고 보는게 맞음
                    return false;
                }
            }

            // 성공 시에는 BootServices가 사라졌으므로
            // memMap 버퍼는 OS가 가져간다고 생각하고 FreePool 안 함.
            return true;
        }

        /// @brief 로드 완료 후 엔트리 주소 반환
        constexpr UINT64 entryPoint() const {
            return entry_;
        }

    private:
        UINT64 entry_ = 0;
    };

} // namespace ribon::loaderPkg
