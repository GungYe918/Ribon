// Ribon - LBPBLoader.hpp (정석 재구현)
#pragma once
#include "LoaderBase.hpp"
#include <Ribon/Elf.hpp>
#include <Ribon/EfiContext.hpp>
#include <Ribon/Memory.hpp>

#include "leyn_bpb.h"

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

            if (eh->e_machine != EM_X86_64)
                return false;

            return true;
        }

        // --------------------------------------------------------------
        // 2) ELF64 커널 로드
        //
        //    - PT_LOAD 세그먼트를 "물리주소"로 간주한 p_paddr(없으면 p_vaddr)에 복사
        //    - BSS 0으로 초기화
        //    - 실제 커널 엔트리를 kernel_entry_에 저장
        //    - entry_에는 "실제 커널 엔트리 주소"를 넣어둔다.
        // --------------------------------------------------------------
        bool load(const void* file, UINTN size) {
            using namespace ribon::mem;

            if (!file || size < sizeof(Elf64_Ehdr))
                return false;

            const auto* eh = reinterpret_cast<const Elf64_Ehdr*>(file);
            if (!IS_ELF(*eh) || !IS_ELF64(*eh))
                return false;

            if (eh->e_machine != EM_X86_64)
                return false;

            const auto* base = reinterpret_cast<const UINT8*>(file);

            if (eh->e_phoff == 0 || eh->e_phnum == 0)
                return false;

            const UINT64 ph_end =
                static_cast<UINT64>(eh->e_phoff) +
                static_cast<UINT64>(eh->e_phnum) * sizeof(Elf64_Phdr);

            if (ph_end > size)
                return false;

            const auto* ph = reinterpret_cast<const Elf64_Phdr*>(base + eh->e_phoff);

            // 커널 로드 영역 추적
            UINT64 phys_min = ~UINT64(0);
            UINT64 phys_max = 0;

            for (UINT16 i = 0; i < eh->e_phnum; ++i) {
                const Elf64_Phdr& p = ph[i];
                if (p.p_type != PT_LOAD)
                    continue;

                if (p.p_memsz == 0)
                    continue;

                if (p.p_offset + p.p_filesz > size)
                    return false; // 잘못된 ELF

                // 물리 목적지: p_paddr가 있으면 우선, 아니면 p_vaddr 사용
                UINT64 phys = (p.p_paddr != 0) ? p.p_paddr : p.p_vaddr;

                UINT8*       dst    = reinterpret_cast<UINT8*>(static_cast<UINTN>(phys));
                const UINT8* src    = base + p.p_offset;
                const UINTN  filesz = static_cast<UINTN>(p.p_filesz);
                const UINTN  memsz  = static_cast<UINTN>(p.p_memsz);

                // 파일 영역 복사
                if (filesz > 0) {
                    Memcpy(dst, src, filesz);
                }

                // BSS 0으로 초기화
                if (memsz > filesz) {
                    Memset(dst + filesz, 0, memsz - filesz);
                }

                UINT64 seg_start = phys;
                UINT64 seg_end   = phys + p.p_memsz;
                if (seg_start < phys_min) phys_min = seg_start;
                if (seg_end   > phys_max) phys_max = seg_end;
            }

            if (phys_max <= phys_min)
                return false;

            // Leyn 커널 엔트리는 BPB 포인터 1개를 인자로 받는 함수로 가정
            kernel_entry_ = reinterpret_cast<LeynKernelEntryFn>(
                static_cast<UINTN>(eh->e_entry)
            );
            if (!kernel_entry_)
                return false;

            // BootLogic/외부 코드가 가져갈 엔트리 주소
            entry_ = static_cast<UINT64>(
                reinterpret_cast<UINTN>(kernel_entry_)
            );

            kernel_phys_start_ = phys_min;
            kernel_phys_end_   = phys_max;

            return true;
        }

        // --------------------------------------------------------------
        // 3) ExitBootServices + Leyn BPB 빌드
        //
        //    - UEFI 메모리 맵 획득 (최종 GetMemoryMap 이후에는 Allocate/Free 금지)
        //    - BPB 버퍼 안에 각종 섹션 구성
        //    - bpb_header_에 BPB 헤더 포인터 저장
        //    - ExitBootServices 호출
        // --------------------------------------------------------------
        bool exitBootServices() {
            using namespace ribon;
            using namespace ribon::mem;

            EFI_BOOT_SERVICES*  bs  = getBS();
            EFI_SYSTEM_TABLE*   st  = getST();
            EFI_HANDLE          ih  = getImageHandle();
            EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = getGop();

            if (!bs || !st || !ih)
                return false;

            if (!kernel_entry_)
                return false;

            EFI_STATUS status;
            UINTN mapSize   = 0;
            UINTN mapKey    = 0;
            UINTN descSize  = 0;
            UINT32 descVer  = 0;

            // ------------------------------------------------------
            // 3-1) 메모리 맵 크기 질의 (첫 GetMemoryMap)
            // ------------------------------------------------------
            status = bs->GetMemoryMap(&mapSize,
                                      nullptr,
                                      &mapKey,
                                      &descSize,
                                      &descVer);
            if (status != EFI_BUFFER_TOO_SMALL) {
                return false;
            }

            // 약간의 여유를 준다
            mapSize += descSize * 2;

            // BPB에 들어갈 최대 섹션 수 (현재 구조 기준 넉넉히)
            constexpr UINTN MAX_SECTIONS = 32;

            // BPB 버퍼 크기 대략 잡기:
            //  - 헤더
            //  - 섹션 테이블
            //  - payload 정렬 여유 + memMap 두 번 정도 + 기타 섹션 여유
            UINTN bpbAllocSize =
                sizeof(leyn_bpb_header) +
                LEYN_BPB_SECTION_TABLE_ALIGN +
                MAX_SECTIONS * sizeof(leyn_bpb_section) +
                LEYN_BPB_SECTION_PAYLOAD_ALIGN +
                mapSize * 2 +
                4096;

            // ------------------------------------------------------
            // 3-2) 커널 영역과 겹치지 않도록 풀 할당 헬퍼
            //      (모든 Allocate/Free는 최종 GetMemoryMap 이전에 끝낸다)
            // ------------------------------------------------------
            auto alloc_non_overlap = [this](EFI_MEMORY_TYPE type,
                                            UINTN size,
                                            void** out) -> EFI_STATUS
            {
                using namespace ribon::mem;

                for (int attempt = 0; attempt < 8; ++attempt) {
                    void* buf = nullptr;
                    EFI_STATUS st = AllocatePool(type, size, &buf);
                    if (EFI_ERROR(st)) return st;

                    UINT64 start = static_cast<UINT64>(reinterpret_cast<UINTN>(buf));
                    UINT64 end   = start + size;

                    bool overlap =
                        !(end <= kernel_phys_start_ || start >= kernel_phys_end_);

                    if (!overlap) {
                        *out = buf;
                        return EFI_SUCCESS;
                    }

                    // 겹치면 버리고 다시 시도
                    FreePool(buf);
                }
                return EFI_OUT_OF_RESOURCES;
            };

            // 3-3) 메모리 맵 버퍼 + BPB 버퍼를 먼저 모두 할당한다
            EFI_MEMORY_DESCRIPTOR* memMap = nullptr;
            void* bpbBuf = nullptr;

            status = alloc_non_overlap(EfiLoaderData, mapSize, (void**)&memMap);
            if (EFI_ERROR(status) || !memMap)
                return false;

            status = alloc_non_overlap(EfiLoaderData, bpbAllocSize, &bpbBuf);
            if (EFI_ERROR(status) || !bpbBuf)
                return false;

            // 3-4) 이제 "최종" GetMemoryMap 을 호출한다.
            //      이 이후로는 Allocate/Free를 하지 않는다.
            status = bs->GetMemoryMap(&mapSize,
                                      memMap,
                                      &mapKey,
                                      &descSize,
                                      &descVer);
            if (EFI_ERROR(status)) {
                // 아직 BootServices 살아있으므로 해제 가능
                FreePool(memMap);
                FreePool(bpbBuf);
                return false;
            }

            // ------------------------------------------------------
            // 3-5) BPB 빌드 (기존 코드 구조 유지, Memcpy/Memset만 사용)
            // ------------------------------------------------------
            UINT8* bpb_start = static_cast<UINT8*>(bpbBuf);
            UINT8* bpb_end   = bpb_start + bpbAllocSize;

            Memset(bpb_start, 0, bpbAllocSize);

            auto* header = reinterpret_cast<leyn_bpb_header*>(bpb_start);
            header->magic         = LEYN_BPB_MAGIC;
            header->version_major = LEYN_BPB_VERSION_MAJOR;
            header->version_minor = LEYN_BPB_VERSION_MINOR;
            header->arch          = LEYN_BPB_ARCH_X86_64;
            header->arch_reserved = 0;
            header->header_size   = sizeof(leyn_bpb_header);
            header->total_size    = 0;   // 나중에 채움
            header->section_count = 0;

            uint32_t hdr_flags = 0;
            hdr_flags |= LEYN_BPB_FLAG_LITTLE_ENDIAN;
            hdr_flags |= LEYN_BPB_FLAG_64BIT_ADDR;
            hdr_flags |= LEYN_BPB_FLAG_EFI_BOOT;
            header->flags = hdr_flags;

            // 커널 엔트리 (가상 주소)
            header->kernel_entry_vaddr =
                static_cast<uint64_t>(reinterpret_cast<UINTN>(kernel_entry_));
            header->flags |= LEYN_BPB_FLAG_ENTRY_VIRT_VALID;
            header->checksum = 0; // 아직 체크섬 미사용

            auto align_up = [](UINTN v, UINTN a) -> UINTN {
                return (v + (a - 1)) & ~(a - 1);
            };

            UINTN cur_off = sizeof(leyn_bpb_header);
            cur_off = align_up(cur_off, LEYN_BPB_SECTION_TABLE_ALIGN);

            if (bpb_start + cur_off > bpb_end) {
                // BootServices 살아있으므로 해제 가능
                FreePool(memMap);
                FreePool(bpbBuf);
                return false;
            }

            auto* section_array = reinterpret_cast<leyn_bpb_section*>(bpb_start + cur_off);
            UINT32 section_count = 0;

            UINTN sec_table_bytes = MAX_SECTIONS * sizeof(leyn_bpb_section);
            cur_off += sec_table_bytes;
            cur_off = align_up(cur_off, LEYN_BPB_SECTION_PAYLOAD_ALIGN);

            if (bpb_start + cur_off > bpb_end) {
                FreePool(memMap);
                FreePool(bpbBuf);
                return false;
            }

            UINT8* payload_cursor = bpb_start + cur_off;

            auto ensure_space = [&](UINTN need) -> bool {
                return (payload_cursor + need) <= bpb_end;
            };

            // 헬퍼: 섹션 추가 (payload 복사)
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
                    Memcpy(payload_cursor, payload, payload_size);
                }

                payload_cursor += payload_size;
                return true;
            };

            // ---------------- BASIC_MEMINFO ----------------
            leyn_bpb_basic_meminfo basic{};
            basic.mem_lower_kb = 640; // 전통적인 BIOS 값

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
                            uint64_t upper_start =
                                (start < 0x100000ULL) ? 0x100000ULL : start;
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
                FreePool(memMap);
                FreePool(bpbBuf);
                return false;
            }

            // ---------------- MEMORY_MAP (leyn e820 스타일) ----------------
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
                UINTN max_entries = mapSize / descSize;
                UINTN aligned_off = align_up(
                    static_cast<UINTN>(payload_cursor - bpb_start),
                    8
                );
                UINT8* mm_ptr = bpb_start + aligned_off;
                payload_cursor = mm_ptr;

                if (!ensure_space(max_entries * sizeof(leyn_bpb_mmap_entry))) {
                    FreePool(memMap);
                    FreePool(bpbBuf);
                    return false;
                }

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

                if (section_count >= MAX_SECTIONS) {
                    FreePool(memMap);
                    FreePool(bpbBuf);
                    return false;
                }

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
                sec.aux_data          = static_cast<uint64_t>(count);

                payload_cursor = mm_ptr + count * sizeof(leyn_bpb_mmap_entry);
            }

            // ---------------- EFI_MEMORY_MAP (원본 + meta) ----------------
            {
                leyn_bpb_efi_mmap_meta meta{};
                meta.descriptor_size    = static_cast<uint32_t>(descSize);
                meta.descriptor_version = descVer;

                UINTN meta_size  = sizeof(meta);
                UINTN total_size = meta_size + mapSize;

                UINTN aligned_off = align_up(
                    static_cast<UINTN>(payload_cursor - bpb_start),
                    8
                );
                UINT8* mm_ptr = bpb_start + aligned_off;
                payload_cursor = mm_ptr;

                if (!ensure_space(total_size)) {
                    FreePool(memMap);
                    FreePool(bpbBuf);
                    return false;
                }

                Memcpy(mm_ptr, &meta, meta_size);
                Memcpy(mm_ptr + meta_size, memMap, mapSize);

                if (section_count >= MAX_SECTIONS) {
                    FreePool(memMap);
                    FreePool(bpbBuf);
                    return false;
                }

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
                sec.aux_data          = static_cast<uint64_t>(mapSize);

                payload_cursor = mm_ptr + total_size;
            }

            // ---------------- EFI_SYSTEM_TABLE 포인터 ----------------
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
                    FreePool(memMap);
                    FreePool(bpbBuf);
                    return false;
                }

                header->flags |= LEYN_BPB_FLAG_HAS_EFI_SYSTEM_TABLE;
            }

            // ---------------- EFI_IMAGE_HANDLE 포인터 ----------------
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
                    FreePool(memMap);
                    FreePool(bpbBuf);
                    return false;
                }

                header->flags |= LEYN_BPB_FLAG_HAS_EFI_IMAGE_HANDLE;
            }

            // ---------------- FRAMEBUFFER_INFO + UEFI_GOP_MODE ----------------
            if (gop && gop->Mode && gop->Mode->Info) {
                EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = gop->Mode->Info;
                EFI_GRAPHICS_PIXEL_FORMAT pfmt = info->PixelFormat;

                leyn_bpb_fb_info fb{};
                fb.framebuffer_addr =
                    static_cast<uint64_t>(gop->Mode->FrameBufferBase);
                fb.framebuffer_width  = info->HorizontalResolution;
                fb.framebuffer_height = info->VerticalResolution;

                fb.framebuffer_bpp   = 32;
                fb.framebuffer_pitch =
                    info->PixelsPerScanLine * (fb.framebuffer_bpp / 8);

                fb.framebuffer_type = LEYN_BPB_FB_TYPE_RGB;
                fb.backend          = LEYN_BPB_FB_BACKEND_UEFI_GOP;

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
                    set_rgb(16, 8, 8, 8, 0, 8);
                } else if (pfmt == PixelBlueGreenRedReserved8BitPerColor) {
                    set_rgb(0, 8, 8, 8, 16, 8);
                } else if (pfmt == PixelBitMask) {
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
                    FreePool(memMap);
                    FreePool(bpbBuf);
                    return false;
                }

                header->flags |= LEYN_BPB_FLAG_HAS_FB_INFO;

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

            // ---------------- BOOTLOADER_NAME / BOOT_CMDLINE ----------------
            {
                const char name[] = "Ribon EFI Bootloader";
                add_section_copy(
                    LEYN_BPB_SEC_BOOTLOADER_NAME,
                    LEYN_BPB_SEC_FLAG_CORE |
                    LEYN_BPB_SEC_FLAG_RUNTIME,
                    1,
                    name,
                    sizeof(name)
                );
            }

            {
                const char cmdline[] = "leyn";
                add_section_copy(
                    LEYN_BPB_SEC_BOOT_CMDLINE,
                    LEYN_BPB_SEC_FLAG_OPTIONAL |
                    LEYN_BPB_SEC_FLAG_RUNTIME,
                    1,
                    cmdline,
                    sizeof(cmdline)
                );
            }

            // ---------------- BPB 헤더 마무리 ----------------
            UINTN used_size = static_cast<UINTN>(payload_cursor - bpb_start);
            header->section_count = section_count;
            header->total_size    = static_cast<uint32_t>(used_size);

            // 커널에 넘길 BPB 포인터 보관
            bpb_header_ = header;

            // 여기까지는 BootServices를 사용했으므로 실패 시에는 FreePool 가능
            // 하지만 이제 ExitBootServices를 호출한 뒤로는 더 이상 FreePool을 호출하지 않는다.

            status = bs->ExitBootServices(ih, mapKey);
            if (EFI_ERROR(status)) {
                // 실패했다면, 아직 BootServices가 살아있으므로 정리 가능
                bpb_header_ = nullptr;
                FreePool(memMap);
                FreePool(bpbBuf);
                return false;
            }

            // 성공: 여기서부터 BootServices는 죽었고,
            // memMap / bpbBuf(EfiLoaderData)는 OS 몫
            return true;
        }

        /// @brief BootLogic/EfiMain이 사용할 엔트리 주소 (실제 커널 엔트리)
        constexpr UINT64 entryPoint() const {
            return entry_;
        }

        /// @brief 커널에 넘길 BPB 헤더 포인터
        const leyn_bpb_header* bpb() const {
            return bpb_header_;
        }

    private:
        using LeynKernelEntryFn = void (*)(const leyn_bpb_header*);

        UINT64 entry_ = 0;

        UINT64 kernel_phys_start_ = 0;
        UINT64 kernel_phys_end_   = 0;

        LeynKernelEntryFn  kernel_entry_ = nullptr;
        leyn_bpb_header*   bpb_header_   = nullptr;
    };

} // namespace ribon::loaderPkg
