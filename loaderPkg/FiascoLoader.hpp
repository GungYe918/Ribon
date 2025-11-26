// loaderPkg/FiascoLoader.hpp
#pragma once

#include <Uefi.h>

#include "LoaderBase.hpp"
#include <Ribon/Elf.hpp>
#include <Ribon/EfiContext.hpp>
#include <Ribon/Memory.hpp>

namespace ribon::loaderPkg {

    /**
     * @brief Fiasco / L4Re 계열 커널용 ELF 로더
     *
     * - CRTP 기반 (LoaderBase<FiascoLoader> 상속)
     * - 현재 버전은 "ELF64 커널을 물리 주소 p_vaddr 로 로드하고 e_entry 로 점프"까지 담당
     * - Fiasco 전용 부트 구조(KIP / sigma0 / roottask / 모듈 전달)는 다음 단계에서 확장
     */
    class FiascoLoader : public LoaderBase<FiascoLoader>
    {
    public:
        constexpr FiascoLoader() = default;

        /**
         * @brief Fiasco 커널 후보인지 탐지
         *
         * 작동 로직:
         *  1) ELF64 인지 확인
         *  2) 파일 앞부분(최대 64KiB)에서 "L4Re" / "Fiasco" 문자열이 보이면 가중치 높게 true
         *  3) 그 외엔 "ELF64면 일단 true" 정도로 완화
         *
         * TODO:
         * 실제 Fiasco 바이너리를 분석 후
         *  - KIP magic ("L4µK")가 포함된 page를 찾거나 
         *  - 특정 섹션/심볼 이름을 기준으로 probe를 강화하는 방식을 추후 추현
         */
        constexpr bool probe(const void* file, UINTN size) const {
            if (!file || size < sizeof(Elf64_Ehdr))
                return false;

            const auto* eh = reinterpret_cast<const Elf64_Ehdr*>(file);

            // 1) ELF64 여부 확인
            if (!IS_ELF(*eh) || !IS_ELF64(*eh))
                return false;

            // 여기서부터는 "Fiasco일 가능성이 있나?" 정도의 heuristic.
            // 너무 aggressive 하게 가면 안 되므로, 일단은 보수적인 형태로 둔다.

            const char* bytes = reinterpret_cast<const char*>(file);
            const UINTN scan_limit = (size < 65536u) ? size : 65536u;

            // 간단한 문자열 탐색: "Fiasco" / "L4Re"
            bool seen_fiasco = false;
            bool seen_l4re   = false;

            for (UINTN i = 0; i + 6 < scan_limit; ++i) {
                // "Fiasco"
                if (!seen_fiasco &&
                    bytes[i + 0] == 'F' &&
                    bytes[i + 1] == 'i' &&
                    bytes[i + 2] == 'a' &&
                    bytes[i + 3] == 's' &&
                    bytes[i + 4] == 'c' &&
                    bytes[i + 5] == 'o')
                {
                    seen_fiasco = true;
                }

                // "L4Re"
                if (!seen_l4re &&
                    bytes[i + 0] == 'L' &&
                    bytes[i + 1] == '4' &&
                    bytes[i + 2] == 'R' &&
                    bytes[i + 3] == 'e')
                {
                    seen_l4re = true;
                }

                if (seen_fiasco && seen_l4re)
                    break;
            }

            // heuristic:
            //  - Fiasco/L4Re 문자열이 보이면 "확실히 Fiasco 후보"
            //  - 안 보이더라도 "ELF64면 일단 로딩 후보"로 true 반환
            //    (실제 환경에서는 BootLogic에서 등록 순서로 우선순위 조절)
            (void)seen_fiasco;
            (void)seen_l4re;

            return true;
        }

        /**
         * @brief ELF64 커널 로드
         *
         *  - ELF 매직 및 클래스 검사
         *  - 각 PT_LOAD 세그먼트를 p_vaddr(= 물리/가상 주소)로 복사
         *  - 파일 크기 < 메모리 크기인 경우 BSS 영역을 0으로 초기화
         *
         * Fiasco 커널은 보통 "자기 자신이 기대하는 물리/가상 베이스 주소"로 링크되어 있고,
         * 여기서는 그대로 p_vaddr 에 로드한다고 가정한다.
         */
        bool load(const void* file, UINTN size)
        {
            if (!file || size < sizeof(Elf64_Ehdr))
                return false;

            const auto* eh = reinterpret_cast<const Elf64_Ehdr*>(file);
            if (!IS_ELF(*eh) || !IS_ELF64(*eh))
                return false;

            // 엔트리 주소 저장
            entry_ = eh->e_entry;

            const auto* base = reinterpret_cast<const UINT8*>(file);
            const auto* ph   = reinterpret_cast<const Elf64_Phdr*>(base + eh->e_phoff);

            // 보호: program header 개수 / 오프셋이 파일 범위 내인지 대략 체크
            if (eh->e_phoff + eh->e_phnum * sizeof(Elf64_Phdr) > size)
                return false;

            for (UINTN i = 0; i < eh->e_phnum; ++i) {
                const auto& p = ph[i];

                if (p.p_type != PT_LOAD)
                    continue;

                // 목적지 주소 (커널이 기대하는 vaddr. 보통 물리=가상으로 맞춰짐)
                UINT8* dst = reinterpret_cast<UINT8*>(static_cast<UINTN>(p.p_vaddr));

                // 소스: 파일 내 오프셋
                if (p.p_offset + p.p_filesz > size)
                    return false; // 잘못된 ELF

                const UINT8* src    = base + p.p_offset;
                const UINTN  filesz = static_cast<UINTN>(p.p_filesz);
                const UINTN  memsz  = static_cast<UINTN>(p.p_memsz);

                // 파일 영역 복사
                if (filesz > 0) {
                    ribon::mem::Memcpy(dst, src, filesz);
                }

                // BSS 영역 0으로 초기화
                if (memsz > filesz) {
                    const UINTN bss_size = memsz - filesz;
                    ribon::mem::Memset(dst + filesz, 0, bss_size);
                }
            }

            return true;
        }

        /**
         * @brief ExitBootServices 호출
         *
         * UEFI 규약에 맞게 MemoryMap을 두 번 얻어오는 패턴.
         * MultibootLoader와 완전히 동일하게 구현.
         */
        bool exitBootServices()
        {
            auto* bs = ribon::getBS();
            auto  ih = ribon::getImageHandle();

            if (!bs || !ih)
                return false;

            UINTN  mapSize  = 0;
            UINTN  mapKey   = 0;
            UINTN  descSize = 0;
            UINT32 descVer  = 0;
            EFI_STATUS st;

            // 1) 크기 질의
            st = bs->GetMemoryMap(&mapSize, nullptr, &mapKey, &descSize, &descVer);
            if (st != EFI_BUFFER_TOO_SMALL)
                return false;

            // 2) 버퍼 할당
            EFI_MEMORY_DESCRIPTOR* memMap = nullptr;
            st = ribon::mem::AllocatePool(EfiLoaderData, mapSize, (void**)&memMap);
            if (EFI_ERROR(st) || !memMap)
                return false;

            // 3) 실제 메모리맵 얻기
            st = bs->GetMemoryMap(&mapSize, memMap, &mapKey, &descSize, &descVer);
            if (EFI_ERROR(st)) {
                ribon::mem::FreePool(memMap);
                return false;
            }

            // 4) ExitBootServices 호출
            st = bs->ExitBootServices(ih, mapKey);
            if (EFI_ERROR(st)) {
                // 실패 → 다시 시도
                ribon::mem::FreePool(memMap);

                mapSize = 0;
                st = bs->GetMemoryMap(&mapSize, nullptr, &mapKey, &descSize, &descVer);
                if (st != EFI_BUFFER_TOO_SMALL)
                    return false;

                st = ribon::mem::AllocatePool(EfiLoaderData, mapSize, (void**)&memMap);
                if (EFI_ERROR(st) || !memMap)
                    return false;

                st = bs->GetMemoryMap(&mapSize, memMap, &mapKey, &descSize, &descVer);
                if (EFI_ERROR(st)) {
                    ribon::mem::FreePool(memMap);
                    return false;
                }

                st = bs->ExitBootServices(ih, mapKey);
                if (EFI_ERROR(st)) {
                    // 여기서부터는 BootServices에 더 이상 의존하지 않는게 안전
                    return false;
                }
            }

            // 성공 시에는 커널이 memMap을 가져간다고 가정, FreePool 하지 않음.
            return true;
        }

        /// @brief 로드 완료 후 엔트리 주소 반환
        constexpr UINT64 entryPoint() const
        {
            return entry_;
        }

    private:
        UINT64 entry_ = 0;
    };

} // namespace ribon::loaderPkg
