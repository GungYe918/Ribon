#include <Ribon/arch/KernelJump.hpp>
#include <Ribon/Elf.hpp>
#include <Ribon/Memory.hpp>

namespace ribon::arch {
namespace {

constexpr UINT64 kPageSize = 4096;
constexpr UINT64 kEntries = 512;
constexpr UINT64 kL1Block = 0x40000000;
constexpr UINT64 kL2Block = 0x200000;
constexpr UINT64 kDescValid = (1ull << 0);
constexpr UINT64 kDescTable = (1ull << 1);
constexpr UINT64 kDescAf = (1ull << 10);
constexpr UINT64 kDescInnerShareable = (3ull << 8);
constexpr UINT64 kAttrNormal = (0ull << 2);
constexpr UINT64 kMairValue = 0x00000000000004FFull;
constexpr UINT64 kTcrBase =
    static_cast<UINT64>(16u) | (16ull << 16) |
    (1ull << 8) | (1ull << 10) | (3ull << 12) |
    (1ull << 24) | (1ull << 26) | (3ull << 28) | (2ull << 30);
constexpr UINT32 kTcrIpsShift = 32;
constexpr UINTN kHighL3Tables = 8;
constexpr UINTN kPageTablePages = 5 + kHighL3Tables;

UINT64 AlignDown(UINT64 value, UINT64 alignment) {
    return value & ~(alignment - 1);
}

UINT64 AlignUp(UINT64 value, UINT64 alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

UINT64 TableDesc(UINT64 phys) {
    return phys | kDescValid | kDescTable;
}

UINT64 BlockDesc(UINT64 phys) {
    return phys | kAttrNormal | kDescInnerShareable | kDescAf | kDescValid;
}

UINT64 PageDesc(UINT64 phys) {
    return phys | kAttrNormal | kDescInnerShareable | kDescAf | kDescValid | kDescTable;
}

UINT64 ReadIdAa64Mmfr0El1() {
    UINT64 value = 0;
    asm volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(value));
    return value;
}

UINT64 PaBitsToTcrIps(UINT32 bits) {
    if (bits <= 32) return 0;
    if (bits <= 36) return 1;
    if (bits <= 40) return 2;
    if (bits <= 42) return 3;
    if (bits <= 44) return 4;
    return 5;
}

UINT32 CurrentPaBits() {
    switch (ReadIdAa64Mmfr0El1() & 0xFu) {
    case 0: return 32;
    case 1: return 36;
    case 2: return 40;
    case 3: return 42;
    case 4: return 44;
    default: return 48;
    }
}

} // namespace

UINT16 CurrentElfMachine() {
    return EM_AARCH64;
}

UINT32 CurrentElfPageSize() {
    return 4096;
}

ribon::boot::BootArch CurrentBootArch() {
    return ribon::boot::BootArch::AArch64;
}

bool PrepareDirectHighEntry(const ribon::boot::LoadedKernelImage& image, KernelJumpRequest& request) {
    if (image.high_entry_vaddr == 0 || image.high_entry_load_addr == 0 ||
        image.linked_vaddr_end <= image.high_entry_vaddr ||
        image.linked_paddr_start == 0 ||
        image.high_entry_load_addr < image.linked_paddr_start) {
        return false;
    }

    const UINT64 high_start = AlignDown(image.high_entry_vaddr, kPageSize);
    const UINT64 high_end = AlignUp(image.linked_vaddr_end, kPageSize);
    const UINT64 high_load =
        AlignDown(image.phys_start + (image.high_entry_load_addr - image.linked_paddr_start),
                  kPageSize);
    const UINT64 high_l2_start = AlignDown(high_start, kL2Block);
    const UINT64 high_l2_end = AlignUp(high_end, kL2Block);
    const UINT64 l2_span = (high_l2_end - high_l2_start) >> 21;
    if (high_end <= high_start || l2_span == 0 || l2_span > kHighL3Tables ||
        (((high_end - 1) >> 30) & 0x1FFu) != ((high_start >> 30) & 0x1FFu)) {
        return false;
    }

    EFI_PHYSICAL_ADDRESS table_phys = 0;
    if (EFI_ERROR(ribon::mem::AllocatePages(
            AllocateAnyPages,
            EfiLoaderData,
            kPageTablePages,
            &table_phys)) ||
        table_phys == 0) {
        return false;
    }
    auto *tables = reinterpret_cast<UINT64*>(static_cast<UINTN>(table_phys));
    ribon::mem::Memset(tables, 0, static_cast<UINTN>(kPageTablePages * kPageSize));

    UINT64 *identity_l0 = tables;
    UINT64 *identity_l1 = tables + kEntries;
    UINT64 *kernel_l0 = tables + (kEntries * 2);
    UINT64 *kernel_l1 = tables + (kEntries * 3);
    UINT64 *kernel_l2 = tables + (kEntries * 4);
    UINT64 *kernel_l3 = tables + (kEntries * 5);

    identity_l0[0] = TableDesc(table_phys + kPageSize);
    for (UINT32 i = 0; i < 4; ++i) {
        identity_l1[i] = BlockDesc(static_cast<UINT64>(i) * kL1Block);
    }

    const UINT32 l0_index = static_cast<UINT32>((high_start >> 39) & 0x1FFu);
    const UINT32 l1_index = static_cast<UINT32>((high_start >> 30) & 0x1FFu);
    const UINT32 start_l2 = static_cast<UINT32>((high_l2_start >> 21) & 0x1FFu);
    kernel_l0[l0_index] = TableDesc(table_phys + kPageSize * 3);
    kernel_l1[l1_index] = TableDesc(table_phys + kPageSize * 4);
    for (UINT64 i = 0; i < l2_span; ++i) {
        kernel_l2[start_l2 + i] = TableDesc(table_phys + kPageSize * (5 + i));
    }
    for (UINT64 va = high_start; va < high_end; va += kPageSize) {
        const UINT64 phys = high_load + (va - high_start);
        const UINT32 l2_index = static_cast<UINT32>((va >> 21) & 0x1FFu);
        const UINT32 l3_index = static_cast<UINT32>((va >> 12) & 0x1FFu);
        const UINT32 table_index = l2_index - start_l2;
        kernel_l3[table_index * kEntries + l3_index] = PageDesc(phys);
    }

    request.high_entry = image.high_entry_vaddr;
    request.high_entry_load = high_load;
    request.high_vaddr_start = high_start;
    request.high_vaddr_end = high_end;
    request.high_load_start = high_load;
    request.high_load_end = high_load + (high_end - high_start);
    request.arch_bootstrap0 = table_phys;
    request.arch_bootstrap1 = table_phys + kPageSize * 2;
    request.arch_bootstrap2 = kTcrBase | (PaBitsToTcrIps(CurrentPaBits()) << kTcrIpsShift);
    request.arch_bootstrap3 = kMairValue;
    request.entry_flags = kKernelEntryFlagEnteredHigh | kKernelEntryFlagRibonDirectHigh;
    return true;
}

[[noreturn]] void JumpToKernel(const KernelJumpRequest& request) {
    if (request.direct_high_enabled &&
        request.arch_bootstrap0 != 0 &&
        request.arch_bootstrap1 != 0 &&
        request.high_entry != 0) {
        asm volatile(
            "msr daifset, #0xf\n"
            "dsb sy\n"
            "mov x0, %0\n"
            "mov x1, %1\n"
            "mov x16, %2\n"
            "msr mair_el1, %6\n"
            "msr tcr_el1, %5\n"
            "msr ttbr0_el1, %3\n"
            "msr ttbr1_el1, %4\n"
            "isb\n"
            "tlbi vmalle1\n"
            "dsb sy\n"
            "isb\n"
            "mrs x9, sctlr_el1\n"
            "orr x9, x9, #(1 << 0)\n"
            "orr x9, x9, #(1 << 2)\n"
            "orr x9, x9, #(1 << 12)\n"
            "msr sctlr_el1, x9\n"
            "isb\n"
            "br x16\n"
            :
            : "r"(request.arg), "r"(static_cast<UINT64>(request.entry_flags)),
              "r"(request.high_entry), "r"(request.arch_bootstrap0),
              "r"(request.arch_bootstrap1), "r"(request.arch_bootstrap2),
              "r"(request.arch_bootstrap3)
            : "x0", "x1", "x9", "x16", "memory");
    }

    asm volatile(
        "msr daifset, #0xf\n"
        "mov x0, %0\n"
        "mov x16, %1\n"
        "dsb sy\n"
        "ic ialluis\n"
        "dsb sy\n"
        "isb\n"
        "br x16\n"
        :
        : "r"(request.arg), "r"(request.fallback_entry)
        : "x0", "x16", "memory");

    __builtin_unreachable();
}

} // namespace ribon::arch
