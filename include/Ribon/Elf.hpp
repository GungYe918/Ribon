#ifndef RIBON_ELF_HPP
#define RIBON_ELF_HPP

#include <Uefi.h>


// ELF 식별자 크기
#define EI_NIDENT 16


// e_ident[] 인덱스 정의 - ELF 헤더 가장 앞 16바이트 구성
enum ElfIdentIndex : UINT8 {
    EI_MAG0 = 0,        // 매직 '0x7F'
    EI_MAG1 = 1,        // 'E'
    EI_MAG2 = 2,        // 'L'
    EI_MAG3 = 3,        // 'F'
    EI_CLASS = 4,       // 파일 클래스 (32/64비트)
    EI_DATA = 5,        // 엔디안 정보 (LSB/MSB)
    EI_VERSION = 6,     // ELF 버전
    EI_OSABI = 7,       // OS/ABI 식별자
    EI_ABIVERSION = 8,  // ABI 버전
    EI_PAD = 9          // 패딩 시작
};

// --------------------
//  ELF 매직 번호
// --------------------
#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'


// --------------------------------
//  ELF 클래스 (32비트 / 64비트)
// --------------------------------
enum ElfClass : UINT8 {
    ELFCLASSNONE = 0,   // 잘못된 클래스
    ELFCLASS32   = 1,   // ELF32
    ELFCLASS64   = 2    // ELF64
};


// ---------------
//  엔디안 정보
// ---------------
enum ElfDataEncoding : UINT8 {
    ELFDATANONE = 0,    // 잘못됨
    ELFDATA2LSB = 1,    // Little Endian (x86 계열)
    ELFDATA2MSB = 2     // Big Endian
};


// --------------------------
//  ELF 타입 (파일의 성격)
// --------------------------
enum ElfType : UINT16 {
    ET_NONE = 0,        // 타입 없음
    ET_REL  = 1,        // 재배치 가능 오브젝트(.o)
    ET_EXEC = 2,        // 실행 파일
    ET_DYN  = 3,        // 공유 라이브러리
    ET_CORE = 4         // 코어 덤프
};


// ----------------------
//  머신 아키텍처 종류
// ----------------------
enum ElfMachine : UINT16 {
    EM_NONE   = 0,
    EM_386    = 3,      // IA-32
    EM_X86_64 = 62,     // AMD64
    EM_AARCH64 = 183    // ARM64
};

// ------------------------------------------
//  OSABI 종류
//     - 대부분 SYSV(0) 또는 STANDALONE(255)
// ------------------------------------------
enum ElfOSABI : UINT8 {
    ELFOSABI_SYSV       = 0,
    ELFOSABI_LINUX      = 3,
    ELFOSABI_STANDALONE = 255  // 부트로더/커스텀 커널에서 사용
};



#pragma pack(push, 1)


// --------------------
//  ELF64 헤더 구조
// --------------------
typedef struct {
    UINT8   e_ident[EI_NIDENT]; // ELF 식별자 (매직, 클래스, 엔디안)
    UINT16  e_type;             // ELF 타입 (exec/reloc/dyn)
    UINT16  e_machine;          // 아키텍처(x86_64 등)
    UINT32  e_version;          // ELF 버전
    UINT64  e_entry;            // 실행 진입점 (커널 엔트리)
    UINT64  e_phoff;            // 프로그램 헤더 오프셋
    UINT64  e_shoff;            // 섹션 헤더 오프셋
    UINT32  e_flags;            // 아키텍처 전용 플래그
    UINT16  e_ehsize;           // ELF 헤더 크기
    UINT16  e_phentsize;        // 프로그램 헤더 엔트리 크기
    UINT16  e_phnum;            // 프로그램 헤더 개수
    UINT16  e_shentsize;        // 섹션 헤더 엔트리 크기
    UINT16  e_shnum;            // 섹션 헤더 개수
    UINT16  e_shstrndx;         // 섹션 이름 스트링 테이블 위치
} Elf64_Ehdr;

// -----------------------------------------
//  ELF64 프로그램 헤더
//    - 로더가 실제로 메모리에 로드하는 정보
// -----------------------------------------
typedef struct {
    UINT32  p_type;     // 세그먼트 타입 (LOAD/NOTE 등)
    UINT32  p_flags;    // 접근 권한 (R/W/X)
    UINT64  p_offset;   // 파일 내 위치
    UINT64  p_vaddr;    // 메모리에 매핑될 가상 주소
    UINT64  p_paddr;    // 물리 주소(대부분 무시)
    UINT64  p_filesz;   // 파일에서의 크기
    UINT64  p_memsz;    // 메모리에 맵핑될 크기(BSS 포함)
    UINT64  p_align;    // 정렬 요구사항
} Elf64_Phdr;


// ---------------------------------------------
//  ELF64 섹션 헤더
//    - 디버깅/심볼/문자열/NOTE 등 다양한 메타정보
// ---------------------------------------------
typedef struct {
    UINT32 sh_name;       // 섹션 이름 문자열 오프셋
    UINT32 sh_type;       // 섹션 타입 NOTE/STRTAB 등
    UINT64 sh_flags;      // 섹션 플래그 (ALLOC/WRITE/EXECINSTR)
    UINT64 sh_addr;       // 메모리 주소 (로드되는 경우)
    UINT64 sh_offset;     // 파일에서의 위치
    UINT64 sh_size;       // 섹션 크기
    UINT32 sh_link;       // 관련 섹션
    UINT32 sh_info;       // 추가 정보
    UINT64 sh_addralign;  // 정렬
    UINT64 sh_entsize;    // 엔트리 크기 (심볼 테이블 등)
} Elf64_Shdr;

#pragma pack(pop)


// -----------------------
//   프로그램 헤더 타입
// -----------------------
enum ElfProgramType : UINT32 {
    PT_NULL    = 0,
    PT_LOAD    = 1,          // 메모리에 로드되는 일반 세그먼트
    PT_DYNAMIC = 2,
    PT_INTERP  = 3,
    PT_NOTE    = 4,          // NOTE 영역 (Multiboot2 정보 포함 가능)
    PT_SHLIB   = 5,
    PT_PHDR    = 6,

    // GNU 확장
    PT_GNU_EH_FRAME = 0x6474e550,
    PT_GNU_STACK    = 0x6474e551,
    PT_GNU_RELRO    = 0x6474e552
};



// -------------------------------
//  프로그램 헤더 플래그 (p_flags)
// -------------------------------
enum ElfProgramFlags : UINT32 {
    PF_X  = 0x1, // 실행 가능
    PF_W  = 0x2, // 쓰기 가능
    PF_R  = 0x4, // 읽기 가능

    PF_RWX = (PF_R | PF_W | PF_X)
};


// -------------------------
//  섹션 타입 정의 (sh_type)
// -------------------------
enum ElfSectionType : UINT32 {
    SHT_NULL      = 0,
    SHT_PROGBITS  = 1,
    SHT_SYMTAB    = 2,
    SHT_STRTAB    = 3,
    SHT_RELA      = 4,
    SHT_HASH      = 5,
    SHT_DYNAMIC   = 6,
    SHT_NOTE      = 7,      // NOTE, Multiboot2 NOTE 포함 가능
    SHT_NOBITS    = 8,      // .bss
    SHT_REL       = 9,
    SHT_SHLIB     = 10,
    SHT_DYNSYM    = 11
};


// ---------------------------
//  ELF 매크로 (맨 아래에 배치)
// ---------------------------

/* ELF 매직 검증 */
#define IS_ELF(ehdr) ( \
    (ehdr).e_ident[EI_MAG0] == ELFMAG0 && \
    (ehdr).e_ident[EI_MAG1] == ELFMAG1 && \
    (ehdr).e_ident[EI_MAG2] == ELFMAG2 && \
    (ehdr).e_ident[EI_MAG3] == ELFMAG3 \
)

/* 64비트 ELF인지 */
#define IS_ELF64(ehdr) \
    ((ehdr).e_ident[EI_CLASS] == ELFCLASS64)

/* Little Endian인지 */
#define IS_ELF_LSB(ehdr) \
    ((ehdr).e_ident[EI_DATA] == ELFDATA2LSB)

/* 실행 가능한 ELF인지 */
#define ELF_IS_EXEC(ehdr) \
    ((ehdr).e_type == ET_EXEC || (ehdr).e_type == ET_DYN)

/* 현재 플랫폼이 지원하는 아키텍처인지 */
#define ELF_MACHINE_SUPPORTED(m) \
    ((m) == EM_X86_64 || (m) == EM_386 || (m) == EM_AARCH64)

#define ELF_IS_SUPPORTED_MACHINE(ehdr) \
    ELF_MACHINE_SUPPORTED((ehdr).e_machine)


#endif /* RIBON_ELF_HPP */
