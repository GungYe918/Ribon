#pragma once
#include "LoaderBase.hpp"
#include <Ribon/Elf.hpp>
#include <Ribon/EfiContext.hpp>
#include <Ribon/Memory.hpp>

extern "C" {
    #include "leyn_bpb.h"
}

namespace ribon::loaderPkg::detail {

    // Leyn 커널 엔트리 프로토타입:
    //   void leyn_kernel_main(const leyn_bpb_header* bpb);
    using LeynKernelEntryFn = void (*)(const leyn_bpb_header*);

    // 실제 Leyn 커널 엔트리 주소 (ELF e_entry)
    static LeynKernelEntryFn g_leyn_kernel_entry = nullptr;

    // BPB 헤더 시작 주소 (커널에는 이 포인터를 전달)
    static leyn_bpb_header* g_leyn_bpb = nullptr;

    // Leyn 커널로 최종 점프하는 트램폴린
    // BootLogic::jumpToKernel()은 항상 인자 없는 void(*)를 호출하므로,
    // 여기서 BPB 포인터를 인자로 넣어 Leyn 커널 엔트리를 호출한다.
    static void LeynKernelEntryStub() {
        if (!g_leyn_kernel_entry || !g_leyn_bpb) {
            // 어떻게든 여기 오면 이미 망한 상황이므로 그냥 halt loop
            for (;;) {
            #if defined(__x86_64__) || defined(_M_X64)
                __asm__ volatile("hlt");
            #else
                // 다른 아키텍처일 경우에도 무한 루프
            #endif
            }
        }

        // Leyn 커널 진입 (반환 없음이 정상)
        g_leyn_kernel_entry(g_leyn_bpb);

        // 여기까지 오면 비정상. 그냥 멈춘다.
        for (;;) {
        #if defined(__x86_64__) || defined(_M_X64)
            __asm__ volatile("hlt");
        #else
        #endif
        }
    }

} // namespace ribon::loaderPkg::detail


// --------------------
// LBPBLoader 본체
// --------------------

namespace ribon::loaderPkg {

    class LBPBLoader : public LoaderBase<LBPBLoader> {
    public:
        constexpr LBPBLoader() = default;

        // --------------------------------------------------------------
        // 1) Leyn 커널 후보인지 probe
        //    - ELF64 + EM_X86_64 이면 Leyn 후보로 간주
        // --------------------------------------------------------------
        constexpr bool probe(const void* file, UINTN size) const {
            if (!file || size < sizeof(Elf64_Ehdr))
                return false;

            const auto* eh = reinterpret_cast<const Elf64_Ehdr*>(file);

            if (!IS_ELF(*eh) || !IS_ELF64(*eh))
                return false;

            // 아키텍처 체크 (x86_64)
            if (eh->e_machine != EM_X86_64)
                return false;

            // TODO: 나중에 Leyn 전용 ELF note/section을 넣어서 더 강하게 필터링 가능
            return true;
        }

        // --------------------------------------------------------------
        // 2) ELF64 커널 로드
        //
        //    - PT_LOAD 세그먼트를 p_vaddr로 복사
        //    - BSS 0으로 초기화
        //    - 진짜 커널 엔트리는 g_leyn_kernel_entry에 저장
        //    - BootLogic에는 트램폴린(LeynKernelEntryStub) 주소를 entry_로 넘김
        // --------------------------------------------------------------
        bool load(const void* file, UINTN size) {
            if (!file || size < sizeof(Elf64_Ehdr))
                return false;

            const auto* eh = reinterpret_cast<const Elf64_Ehdr*>(file);
            if (!IS_ELF(*eh) || !IS_ELF64(*eh))
                return false;

            if (eh->e_machine != EM_X86_64)
                return false;

            const auto* base = reinterpret_cast<const UINT8*>(file);
            const auto* ph   = reinterpret_cast<const Elf64_Phdr*>(base + eh->e_phoff);

            // program header 범위 체크
            if (eh->e_phoff + eh->e_phnum * sizeof(Elf64_Phdr) > size)
                return false;

            // 커널 물리 영역 대략 추적 (필요하면 IMAGE_LOAD_BASE 섹션 등에 쓸 수 있음)
            UINT64 phys_min = ~UINT64(0);
            UINT64 phys_max = 0;

            for (UINTN i = 0; i < eh->e_phnum; ++i) {
                const auto& p = ph[i];
                if (p.p_type != PT_LOAD)
                    continue;

                if (p.p_offset + p.p_filesz > size)
                    return false; // 잘못된 ELF

                UINT8*       dst    = reinterpret_cast<UINT8*>(static_cast<UINTN>(p.p_vaddr));
                const UINT8* src    = base + p.p_offset;
                const UINTN  filesz = static_cast<UINTN>(p.p_filesz);
                const UINTN  memsz  = static_cast<UINTN>(p.p_memsz);

                // 파일 영역 복사
                if (filesz > 0) {
                    ribon::mem::Memcpy(dst, src, filesz);
                }

                // BSS 영역 0으로 초기화
                if (memsz > filesz) {
                    ribon::mem::Memset(dst + filesz, 0, memsz - filesz);
                }

                if (p.p_memsz > 0) {
                    UINT64 seg_start = p.p_vaddr;
                    UINT64 seg_end   = p.p_vaddr + p.p_memsz;
                    if (seg_start < phys_min) phys_min = seg_start;
                    if (seg_end   > phys_max) phys_max = seg_end;
                }
            }

            // 진짜 Leyn 커널 엔트리 (BPB를 인자로 받는 함수로 가정)
            detail::g_leyn_kernel_entry = reinterpret_cast<detail::LeynKernelEntryFn>(eh->e_entry);

            // BootLogic이 호출할 엔트리는 트램폴린으로 설정
            entry_ = reinterpret_cast<UINT64>(&detail::LeynKernelEntryStub);

            // (필요하면 phys_min/phys_max를 나중에 aux_data 등에 사용할 수 있음)
            kernel_phys_start_ = phys_min;
            kernel_phys_end_   = phys_max;

            return true;
        }

        // --------------------------------------------------------------
        // 3) ExitBootServices + Leyn BPB 빌드
        //
        //    - EFI 메모리 맵 2단계 획득
        //    - 그 안에서:
        //        * BASIC_MEMINFO
        //        * MEMORY_MAP(e820 스타일)
        //        * EFI_MEMORY_MAP(원본)
        //        * EFI_SYSTEM_TABLE 포인터
        //        * EFI_IMAGE_HANDLE 포인터
        //        * FRAMEBUFFER_INFO (UEFI GOP 기준)
        //        * UEFI_GOP_MODE
        //        * BOOTLOADER_NAME / BOOT_CMDLINE
        //      섹션들을 BPB 버퍼 안에 차례로 구성
        //    - g_leyn_bpb = BPB 헤더 포인터
        //    - ExitBootServices 호출
        // --------------------------------------------------------------
        bool exitBootServices()
        {
            auto* bs = ribon::getBS();
            auto* st = ribon::getST();
            auto  ih = ribon::getImageHandle();
            auto* gop = ribon::getGop();

            if (!bs || !st || !ih)
                return false;

            EFI_STATUS status;
            UINTN mapSize   = 0;
            UINTN mapKey    = 0;
            UINTN descSize  = 0;
            UINT32 descVer  = 0;

            // 1) 메모리 맵 크기 질의
            status = bs->GetMemoryMap(&mapSize, nullptr, &mapKey, &descSize, &descVer);
            if (status != EFI_BUFFER_TOO_SMALL)
                return false;

            // BPB에 메모리 맵도 복사할 거라서 대략 2배 + 여유분 정도 잡는다.
            constexpr UINTN MAX_SECTIONS = 32;
            UINTN bpbAllocSize =
                sizeof(leyn_bpb_header) +
                LEYN_BPB_SECTION_TABLE_ALIGN +
                MAX_SECTIONS * sizeof(leyn_bpb_section) +
                LEYN_BPB_SECTION_PAYLOAD_ALIGN +
                mapSize * 2 + 4096;

            void* bpbBuf = nullptr;
            status = ribon::mem::AllocatePool(EfiLoaderData, bpbAllocSize, &bpbBuf);
            if (EFI_ERROR(status) || !bpbBuf)
                return false;

            EFI_MEMORY_DESCRIPTOR* memMap = nullptr;
            status = ribon::mem::AllocatePool(EfiLoaderData, mapSize, (void**)&memMap);
            if (EFI_ERROR(status) || !memMap) {
                ribon::mem::FreePool(bpbBuf);
                return false;
            }

            // 2) 실제 메모리 맵 획득 (이 이후로는 성공 경로에서는 Allocate/Free 안 하는게 안전)
            status = bs->GetMemoryMap(&mapSize, memMap, &mapKey, &descSize, &descVer);
            if (EFI_ERROR(status)) {
                ribon::mem::FreePool(memMap);
                ribon::mem::FreePool(bpbBuf);
                return false;
            }

            // ------------------------------------------------------------------
            //  BPB 빌드 시작
            // ------------------------------------------------------------------
            UINT8* bpb_start = static_cast<UINT8*>(bpbBuf);
            UINT8* bpb_end   = bpb_start + bpbAllocSize;

            // 전체 버퍼를 0으로 초기화 (선택 사항이지만 디버깅에 유리)
            ribon::mem::Memset(bpb_start, 0, bpbAllocSize);

            // 헤더
            auto* header = reinterpret_cast<leyn_bpb_header*>(bpb_start);
            header->magic         = LEYN_BPB_MAGIC;
            header->version_major = LEYN_BPB_VERSION_MAJOR;
            header->version_minor = LEYN_BPB_VERSION_MINOR;
            header->arch          = LEYN_BPB_ARCH_X86_64;
            header->arch_reserved = 0;
            header->header_size   = sizeof(leyn_bpb_header);
            header->total_size    = 0;      // 나중에 채움
            header->section_count = 0;      // 나중에 채움

            // BPB 플래그 기본값
            uint32_t hdr_flags = 0;
            hdr_flags |= LEYN_BPB_FLAG_LITTLE_ENDIAN;
            hdr_flags |= LEYN_BPB_FLAG_64BIT_ADDR;
            hdr_flags |= LEYN_BPB_FLAG_EFI_BOOT;
            // ExitBootServices()는 이 함수 끝에서 호출하므로
            // 커널 입장에서는 이미 BootServices가 꺼진 상태 → ON 플래그는 세우지 않음

            header->flags = hdr_flags;

            // 커널 엔트리 주소 (가상 주소로 취급)
            header->kernel_entry_vaddr =
                static_cast<uint64_t>(reinterpret_cast<UINTN>(detail::g_leyn_kernel_entry));
            header->flags |= LEYN_BPB_FLAG_ENTRY_VIRT_VALID;
            header->checksum = 0; // 체크섬 미사용 (FLAG_HAS_CHECKSUM 미설정)

            // 헤더 뒤의 섹션 테이블 시작 위치 정렬
            auto align_up = [](UINTN v, UINTN a) -> UINTN {
                return (v + (a - 1)) & ~(a - 1);
            };

            UINTN cur_off = sizeof(leyn_bpb_header);
            cur_off = align_up(cur_off, LEYN_BPB_SECTION_TABLE_ALIGN);

            if (bpb_start + cur_off > bpb_end) {
                // BPB 버퍼가 너무 작음
                return false;
            }

            auto* section_array = reinterpret_cast<leyn_bpb_section*>(bpb_start + cur_off);
            UINT32 section_count = 0;

            // 섹션 테이블 끝 이후 페이로드 시작 위치 정렬
            UINTN sec_table_bytes = MAX_SECTIONS * sizeof(leyn_bpb_section);
            cur_off += sec_table_bytes;
            cur_off = align_up(cur_off, LEYN_BPB_SECTION_PAYLOAD_ALIGN);

            if (bpb_start + cur_off > bpb_end)
                return false;

            UINT8* payload_cursor = bpb_start + cur_off;

            auto ensure_space = [&](UINTN need) -> bool {
                return (payload_cursor + need) <= bpb_end;
            };

            // ------------------------------------------------------------------
            //  헬퍼: 새 섹션 한 개 등록 (payload 미리 있는 경우)
            // ------------------------------------------------------------------
            auto add_section_copy = [&](uint32_t type,
                                        uint16_t flags,
                                        uint16_t alignment,
                                        const void* payload,
                                        UINTN payload_size,
                                        uint64_t aux_data = 0ULL) -> bool
            {
                if (section_count >= MAX_SECTIONS)
                    return false;

                UINTN aligned_off = align_up(
                    static_cast<UINTN>(payload_cursor - bpb_start),
                    alignment ? alignment : 1
                );
                UINT8* aligned_ptr = bpb_start + aligned_off;
                if (aligned_ptr > bpb_end)
                    return false;

                payload_cursor = aligned_ptr;

                if (!ensure_space(payload_size))
                    return false;

                leyn_bpb_section& sec = section_array[section_count++];
                sec.type       = type;
                sec.flags      = flags;
                sec.alignment  = alignment;
                sec.reserved   = 0;
                sec.payload_offset     = static_cast<uint64_t>(payload_cursor - bpb_start);
                sec.payload_size       = static_cast<uint64_t>(payload_size);
                sec.uncompressed_size  = static_cast<uint64_t>(payload_size);
                sec.aux_data           = aux_data;

                if (payload_size && payload) {
                    ribon::mem::Memcpy(payload_cursor, payload, payload_size);
                }

                payload_cursor += payload_size;
                return true;
            };

            // ------------------------------------------------------------------
            //  3-1) BASIC_MEMINFO 섹션
            //       (Multiboot mem_lower/mem_upper에 대응)
            // ------------------------------------------------------------------
            leyn_bpb_basic_meminfo basic{};
            basic.mem_lower_kb = 640; // 통상적인 0~640KB

            // mem_upper_kb = 1MB 이상 usable RAM 합계 / 1KB
            {
                UINTN offset = 0;
                uint64_t upper_bytes = 0;

                while (offset + descSize <= mapSize) {
                    auto* d = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(
                        reinterpret_cast<UINT8*>(memMap) + offset
                    );

                    if (d->Type == EfiConventionalMemory) {
                        uint64_t start = d->PhysicalStart;
                        uint64_t size  = d->NumberOfPages * 4096ULL;
                        uint64_t end   = start + size;

                        if (end > 0x100000ULL) {
                            uint64_t upper_start = (start < 0x100000ULL)
                                                 ? 0x100000ULL
                                                 : start;
                            upper_bytes += (end - upper_start);
                        }
                    }

                    offset += descSize;
                }

                basic.mem_upper_kb = static_cast<uint32_t>(upper_bytes / 1024ULL);
            }

            if (!add_section_copy(
                    LEYN_BPB_SEC_BASIC_MEMINFO,
                    LEYN_BPB_SEC_FLAG_CORE |
                    LEYN_BPB_SEC_FLAG_HOT  |
                    LEYN_BPB_SEC_FLAG_HW_RELATED,
                    8,
                    &basic,
                    sizeof(basic)))
            {
                return false;
            }

            // ------------------------------------------------------------------
            //  3-2) MEMORY_MAP 섹션 (e820 스타일)
            // ------------------------------------------------------------------
            auto map_efi_to_leyn_type = [](UINT32 t) -> uint32_t {
                switch (t) {
                case EfiConventionalMemory:
                    return 1; // usable
                case EfiACPIReclaimMemory:
                    return 3; // ACPI reclaimable
                case EfiACPIMemoryNVS:
                    return 4; // ACPI NVS
                case EfiUnusableMemory:
                    return 5; // bad memory
                default:
                    return 2; // reserved/기타
                }
            };

            {
                // 엔트리 개수 대략
                UINTN max_entries = mapSize / descSize;
                UINTN aligned_off = align_up(
                    static_cast<UINTN>(payload_cursor - bpb_start),
                    8
                );
                UINT8* mm_ptr = bpb_start + aligned_off;
                payload_cursor = mm_ptr;

                if (!ensure_space(max_entries * sizeof(leyn_bpb_mmap_entry)))
                    return false;

                auto* mmap_out = reinterpret_cast<leyn_bpb_mmap_entry*>(mm_ptr);

                UINTN offset = 0;
                UINTN count  = 0;
                while (offset + descSize <= mapSize && count < max_entries) {
                    auto* d = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(
                        reinterpret_cast<UINT8*>(memMap) + offset
                    );

                    mmap_out[count].base_addr = d->PhysicalStart;
                    mmap_out[count].length    = d->NumberOfPages * 4096ULL;
                    mmap_out[count].type      = map_efi_to_leyn_type(d->Type);
                    mmap_out[count].flags     = 0;

                    ++count;
                    offset += descSize;
                }

                if (section_count >= MAX_SECTIONS)
                    return false;

                leyn_bpb_section& sec = section_array[section_count++];
                sec.type      = LEYN_BPB_SEC_MEMORY_MAP;
                sec.flags     = LEYN_BPB_SEC_FLAG_CORE |
                                LEYN_BPB_SEC_FLAG_EARLY_MAP |
                                LEYN_BPB_SEC_FLAG_HW_RELATED;
                sec.alignment = 8;
                sec.reserved  = 0;
                sec.payload_offset    = static_cast<uint64_t>(mm_ptr - bpb_start);
                sec.payload_size      = static_cast<uint64_t>(count * sizeof(leyn_bpb_mmap_entry));
                sec.uncompressed_size = sec.payload_size;
                sec.aux_data          = static_cast<uint64_t>(count); // 엔트리 수 (편의용)

                payload_cursor = mm_ptr + count * sizeof(leyn_bpb_mmap_entry);
            }

            // ------------------------------------------------------------------
            //  3-3) EFI_MEMORY_MAP 섹션 (원본)
            //       payload = [ leyn_bpb_efi_mmap_meta ][ EFI_MEMORY_DESCRIPTOR[]... ]
            // ------------------------------------------------------------------
            {
                leyn_bpb_efi_mmap_meta meta{};
                meta.descriptor_size    = static_cast<uint32_t>(descSize);
                meta.descriptor_version = descVer;

                UINTN meta_size = sizeof(meta);
                UINTN total_size = meta_size + mapSize;

                UINTN aligned_off = align_up(
                    static_cast<UINTN>(payload_cursor - bpb_start),
                    8
                );
                UINT8* mm_ptr = bpb_start + aligned_off;
                payload_cursor = mm_ptr;

                if (!ensure_space(total_size))
                    return false;

                // meta + raw map 복사
                ribon::mem::Memcpy(mm_ptr, &meta, meta_size);
                ribon::mem::Memcpy(mm_ptr + meta_size, memMap, mapSize);

                if (section_count >= MAX_SECTIONS)
                    return false;

                leyn_bpb_section& sec = section_array[section_count++];
                sec.type      = LEYN_BPB_SEC_EFI_MEMORY_MAP;
                sec.flags     = LEYN_BPB_SEC_FLAG_CORE |
                                LEYN_BPB_SEC_FLAG_RUNTIME |
                                LEYN_BPB_SEC_FLAG_SRC_EFI |
                                LEYN_BPB_SEC_FLAG_HW_RELATED;
                sec.alignment = 8;
                sec.reserved  = 0;
                sec.payload_offset    = static_cast<uint64_t>(mm_ptr - bpb_start);
                sec.payload_size      = static_cast<uint64_t>(total_size);
                sec.uncompressed_size = sec.payload_size;
                sec.aux_data          = static_cast<uint64_t>(mapSize); // raw map 크기

                payload_cursor = mm_ptr + total_size;
            }

            // ------------------------------------------------------------------
            //  3-4) EFI_SYSTEM_TABLE 포인터 섹션
            // ------------------------------------------------------------------
            {
                leyn_bpb_efi_system_table_ptr st_ptr{};
                st_ptr.system_table_phys =
                    static_cast<uint64_t>(reinterpret_cast<UINTN>(st));

                if (!add_section_copy(
                        LEYN_BPB_SEC_EFI_SYSTEM_TABLE,
                        LEYN_BPB_SEC_FLAG_CORE |
                        LEYN_BPB_SEC_FLAG_RUNTIME |
                        LEYN_BPB_SEC_FLAG_SRC_EFI,
                        8,
                        &st_ptr,
                        sizeof(st_ptr)))
                {
                    return false;
                }

                header->flags |= LEYN_BPB_FLAG_HAS_EFI_SYSTEM_TABLE;
            }

            // ------------------------------------------------------------------
            //  3-5) EFI_IMAGE_HANDLE 섹션
            // ------------------------------------------------------------------
            {
                leyn_bpb_efi_image_handle_ptr ih_ptr{};
                ih_ptr.image_handle =
                    static_cast<uint64_t>(reinterpret_cast<UINTN>(ih));

                if (!add_section_copy(
                        LEYN_BPB_SEC_EFI_IMAGE_HANDLE,
                        LEYN_BPB_SEC_FLAG_CORE |
                        LEYN_BPB_SEC_FLAG_RUNTIME |
                        LEYN_BPB_SEC_FLAG_SRC_EFI,
                        8,
                        &ih_ptr,
                        sizeof(ih_ptr)))
                {
                    return false;
                }

                header->flags |= LEYN_BPB_FLAG_HAS_EFI_IMAGE_HANDLE;
            }

            // ------------------------------------------------------------------
            //  3-6) FRAMEBUFFER_INFO + UEFI_GOP_MODE 섹션 (GOP가 있을 경우)
            // ------------------------------------------------------------------
            if (gop && gop->Mode && gop->Mode->Info) {
                EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = gop->Mode->Info;
                EFI_GRAPHICS_PIXEL_FORMAT pfmt = info->PixelFormat;

                leyn_bpb_fb_info fb{};
                fb.framebuffer_addr =
                    static_cast<uint64_t>(gop->Mode->FrameBufferBase);
                fb.framebuffer_width  = info->HorizontalResolution;
                fb.framebuffer_height = info->VerticalResolution;

                // 기본 32bpp 가정 (대부분 그렇게 동작)
                fb.framebuffer_bpp   = 32;
                fb.framebuffer_pitch =
                    info->PixelsPerScanLine * (fb.framebuffer_bpp / 8);

                fb.framebuffer_type = LEYN_BPB_FB_TYPE_RGB;
                fb.backend          = LEYN_BPB_FB_BACKEND_UEFI_GOP;

                // RGB 마스크 설정
                auto set_rgb = [&](uint8_t rp, uint8_t rl,
                                   uint8_t gp, uint8_t gl,
                                   uint8_t bp, uint8_t bl) {
                    fb.color.rgb.red_position   = rp;
                    fb.color.rgb.red_mask_size  = rl;
                    fb.color.rgb.green_position = gp;
                    fb.color.rgb.green_mask_size= gl;
                    fb.color.rgb.blue_position  = bp;
                    fb.color.rgb.blue_mask_size = bl;
                    fb.color.rgb.reserved[0]    = 0;
                    fb.color.rgb.reserved[1]    = 0;
                };

                if (pfmt == PixelRedGreenBlueReserved8BitPerColor) {
                    // R:G:B:Res (little-endian 기준)
                    set_rgb(16, 8, 8, 8, 0, 8);
                } else if (pfmt == PixelBlueGreenRedReserved8BitPerColor) {
                    // B:G:R:Res
                    set_rgb(0, 8, 8, 8, 16, 8);
                } else if (pfmt == PixelBitMask) {
                    // BitMask 모드는 Info->PixelInformation 마스크를 사용
                    auto calc_pos_len = [](uint32_t mask, uint8_t& pos, uint8_t& len) {
                        if (!mask) { pos = 0; len = 0; return; }
                        uint8_t p = 0;
                        while (((mask >> p) & 1u) == 0u && p < 32)
                            ++p;
                        uint8_t l = 0;
                        while (((mask >> (p + l)) & 1u) == 1u && (p + l) < 32)
                            ++l;
                        pos = p;
                        len = l;
                    };

                    uint8_t rp=0, rl=0, gp=0, gl=0, bp=0, bl=0;
                    calc_pos_len(info->PixelInformation.RedMask,   rp, rl);
                    calc_pos_len(info->PixelInformation.GreenMask, gp, gl);
                    calc_pos_len(info->PixelInformation.BlueMask,  bp, bl);
                    set_rgb(rp, rl, gp, gl, bp, bl);
                } else {
                    // 알 수 없는 포맷이면 그냥 RGB 8:8:8 기본값
                    set_rgb(16, 8, 8, 8, 0, 8);
                }

                if (!add_section_copy(
                        LEYN_BPB_SEC_FRAMEBUFFER_INFO,
                        LEYN_BPB_SEC_FLAG_CORE |
                        LEYN_BPB_SEC_FLAG_RUNTIME |
                        LEYN_BPB_SEC_FLAG_SRC_EFI |
                        LEYN_BPB_SEC_FLAG_HW_RELATED |
                        LEYN_BPB_SEC_FLAG_HOT,
                        8,
                        &fb,
                        sizeof(fb)))
                {
                    return false;
                }

                header->flags |= LEYN_BPB_FLAG_HAS_FB_INFO;

                // UEFI_GOP_MODE 섹션 (추가 정보)
                leyn_bpb_uefi_gop_mode gop_mode{};
                gop_mode.version              = info->Version;
                gop_mode.horizontal_resolution= info->HorizontalResolution;
                gop_mode.vertical_resolution  = info->VerticalResolution;
                gop_mode.pixels_per_scanline  = info->PixelsPerScanLine;
                gop_mode.pixel_format         = static_cast<uint32_t>(pfmt);
                gop_mode.reserved             = 0;
                gop_mode.framebuffer_base =
                    static_cast<uint64_t>(gop->Mode->FrameBufferBase);
                gop_mode.framebuffer_size =
                    static_cast<uint64_t>(gop->Mode->FrameBufferSize);
                gop_mode.mode_info_phys =
                    static_cast<uint64_t>(reinterpret_cast<UINTN>(info));

                add_section_copy(
                    LEYN_BPB_SEC_UEFI_GOP_MODE,
                    LEYN_BPB_SEC_FLAG_OPTIONAL |
                    LEYN_BPB_SEC_FLAG_RUNTIME |
                    LEYN_BPB_SEC_FLAG_SRC_EFI |
                    LEYN_BPB_SEC_FLAG_HW_RELATED,
                    8,
                    &gop_mode,
                    sizeof(gop_mode)
                );
            }

            // ------------------------------------------------------------------
            //  3-7) BOOTLOADER_NAME / BOOT_CMDLINE 섹션 (문자열)
            // ------------------------------------------------------------------
            {
                const char name[] = "Ribon EFI Bootloader";
                add_section_copy(
                    LEYN_BPB_SEC_BOOTLOADER_NAME,
                    LEYN_BPB_SEC_FLAG_CORE |
                    LEYN_BPB_SEC_FLAG_RUNTIME,
                    1,
                    name,
                    sizeof(name)  // null 포함
                );
            }

            {
                const char cmdline[] = "leyn"; // 필요하면 나중에 실제 커맨드라인 사용
                add_section_copy(
                    LEYN_BPB_SEC_BOOT_CMDLINE,
                    LEYN_BPB_SEC_FLAG_OPTIONAL |
                    LEYN_BPB_SEC_FLAG_RUNTIME,
                    1,
                    cmdline,
                    sizeof(cmdline)
                );
            }

            // TODO: 나중에 ACPI/SMBIOS/NETWORK/CONFIG_GRAPH 등도 여기서 추가 가능

            // ------------------------------------------------------------------
            //  BPB 헤더 마무리
            // ------------------------------------------------------------------
            UINTN used_size = static_cast<UINTN>(payload_cursor - bpb_start);
            header->section_count = section_count;
            header->total_size    = static_cast<uint32_t>(used_size);

            // Leyn 커널에 넘길 전역 포인터 세팅
            detail::g_leyn_bpb = header;

            // ------------------------------------------------------------------
            //  마지막으로 ExitBootServices
            // ------------------------------------------------------------------
            status = bs->ExitBootServices(ih, mapKey);
            if (EFI_ERROR(status)) {
                // 실패 시에는 더 이상 여기서 회복 시도하지 않고 false 반환
                // (필요하면 재시도 로직 추가 가능)
                return false;
            }

            // 성공
            return true;
        }

        /// @brief BootLogic이 사용할 엔트리 주소 반환 (트램폴린)
        constexpr UINT64 entryPoint() const {
            return entry_;
        }

    private:
        UINT64 entry_ = 0;

        // 커널 로드 베이스/끝 주소 (필요시 IMAGE_LOAD_BASE 등에 쓸 수 있도록 저장)
        UINT64 kernel_phys_start_ = 0;
        UINT64 kernel_phys_end_   = 0;
    };

} // namespace ribon::loaderPkg