#pragma once
/**
 * @file Multiboot.hpp
 * @brief Multiboot v1 / v2 공용 헤더 정의
 *
 * 이 파일은 Multiboot1(Legacy GRUB)와
 * Multiboot2(GRUB2, Limine 일부 호환)에서 공통적으로 사용하는
 * 구조체와 플래그를 정의한다.
 *
 * 실제 로딩 절차는 LoaderBase ABI를 구현한
 * MultibootLoader 클래스에서 담당한다.
 */

#include <Uefi.h>

namespace ribon::loaderPkg {

/* ============================================================
 *  MULTIBOOT v1
 * ============================================================*/


/**
 * @brief Multiboot v1 헤더 구조
 *
 * 커널 ELF 또는 바이너리 앞부분에 위치.
 * GRUB(구버전) 등이 이 구조를 스캔하여 커널을 로드한다.
 */
struct MultibootHeader {
    UINT32 magic;           // 항상 0x1BADB002
    UINT32 flags;           // 아래의 MULTIBOOT_FLAG_* 비트 조합
    UINT32 checksum;        // magic + flags + checksum = 0

    // flags[16] 비트 세트 시 아래 필드 유효
    UINT32 header_addr;     // 커널 헤더의 실제 주소
    UINT32 load_addr;       // 로드 시작 주소                 
    UINT32 load_end_addr;   // 로드 끝 주소 (BSS 직전)      
    UINT32 bss_end_addr;    // BSS 끝 주소                   
    UINT32 entry_addr;      // 엔트리 포인터                

    // flags[2] = video 설정
    UINT32 mode_type;     // 0=text, 1=graphics
    UINT32 width;
    UINT32 height;
    UINT32 depth;
};

//------------------------
// Multiboot v1 Flags
//------------------------

#define MULTIBOOT_HEADER_MAGIC      0x1BADB002
#define MULTIBOOT_BOOTLOADER_MAGIC  0x2BADB002

#define MULTIBOOT_FLAG_ALIGN        0x00000001  // 커널을 페이지 경계에 정렬
#define MULTIBOOT_FLAG_MEMINFO      0x00000002  // 메모리 맵 정보 요구
#define MULTIBOOT_FLAG_VIDEO        0x00000004  // 비디오 모드 사용
#define MULTIBOOT_FLAG_AOUT_KLUDGE  0x00010000  // a.out 호환 모드 (ELF 로더 비권장)




/* ============================================================
 *  MULTIBOOT v2
 * ============================================================*/


/**
 * @brief Multiboot2 헤더 (GRUB2 / Limine 등)
 *
 * ELF NOTE 영역 또는 바이너리 상단에 나타남.
 * 다양한 "tag" 구조를 통해 확장성을 제공한다.
 */
struct Multiboot2Header {
    UINT32 magic;           // 0xE85250D6
    UINT32 architecture;    // 0 = i386, 1 = MIPS, etc.
    UINT32 header_length;
    UINT32 checksum;        // 모든 필드의 32bit sum = 0
};

#define MULTIBOOT2_HEADER_MAGIC     0xE85250D6
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289



/**
 * @brief Multiboot2 Tag 기본 구조
 */
struct Multiboot2Tag {
    UINT16 type; // 태그 종류
    UINT16 flags;
    UINT32 size; // 태그 전체 크기
};


// Multiboot2 Tag Types
#define MB2_TAG_END                0
#define MB2_TAG_INFORMATION_REQ    1
#define MB2_TAG_ADDRESS            2
#define MB2_TAG_ENTRY_ADDRESS      3
#define MB2_TAG_FLAGS              4
#define MB2_TAG_FRAMEBUFFER        5
#define MB2_TAG_MODULE             6
#define MB2_TAG_ELF_SECTIONS       9
#define MB2_TAG_APM                10
#define MB2_TAG_VBE                7


/* ============================================================
 *  MULTIBOOT v2: Tag 세부 구조체
 * ============================================================*/

/**
 * @brief Multiboot2 Entry Address Tag
 */
struct MB2TagEntry {
    Multiboot2Tag   tag;
    UINT32          entry_addr; ///< 커널 진입점(물리)
};

/**
 * @brief Multiboot2 Address Tag
 */
struct MB2TagAddress {
    Multiboot2Tag   tag;
    UINT32          header_addr;    // 헤더 주소
    UINT32          load_addr;      // 로드 시작
    UINT32          load_end_addr;  // 로드 끝
    UINT32          bss_end_addr;   // BSS 끝
};

/**
 * @brief Multiboot2 Framebuffer Tag
 */
struct MB2TagFramebuffer {
    Multiboot2Tag   tag;
    UINT64          framebuffer_addr;
    UINT32          framebuffer_pitch;
    UINT32          framebuffer_width;
    UINT32          framebuffer_height;
    UINT8           framebuffer_bpp;   // bit per pixel
    UINT8           framebuffer_type;  // 0=palette, 1=RGB
    UINT16          reserved;
};



/* ============================================================
 *  MULTIBOOT 검증 매크로
 * ============================================================*/

#define IS_MULTIBOOT1_HEADER(h) \
    ((h).magic == MULTIBOOT_HEADER_MAGIC)

#define IS_MULTIBOOT2_HEADER(h) \
    ((h).magic == MULTIBOOT2_HEADER_MAGIC)

#define IS_MULTIBOOT1_BOOT_MAGIC(x) \
    ((x) == MULTIBOOT_BOOTLOADER_MAGIC)

#define IS_MULTIBOOT2_BOOT_MAGIC(x) \
    ((x) == MULTIBOOT2_BOOTLOADER_MAGIC)


} // namespace ribon::loaderPkg