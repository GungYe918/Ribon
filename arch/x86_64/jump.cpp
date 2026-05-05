#include <Ribon/arch/KernelJump.hpp>
#include <Ribon/Elf.hpp>
#include <Ribon/Memory.hpp>

namespace ribon::arch {
namespace {

constexpr UINT64 kPageSize = 4096;
constexpr UINT64 kEntries = 512;
constexpr UINT64 kLarge2M = 0x200000;
constexpr UINT64 kLarge1G = 0x40000000;
constexpr UINT64 kPtePresent = (1ull << 0);
constexpr UINT64 kPteWrite = (1ull << 1);
constexpr UINT64 kPteLarge = (1ull << 7);
constexpr UINT64 kAddrMask = 0x000FFFFFFFFFF000ull;
constexpr UINTN kPageTablePages = 8;

UINT64 AlignDown(UINT64 value, UINT64 alignment) {
    return value & ~(alignment - 1);
}

UINT64 AlignUp(UINT64 value, UINT64 alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

UINT64 TableDesc(UINT64 phys) {
    return (phys & kAddrMask) | kPtePresent | kPteWrite;
}

UINT64 LargeDesc(UINT64 phys) {
    return (phys & kAddrMask) | kPtePresent | kPteWrite | kPteLarge;
}

} // namespace

UINT16 CurrentElfMachine() {
    return EM_X86_64;
}

UINT32 CurrentElfPageSize() {
    return 4096;
}

ribon::boot::BootArch CurrentBootArch() {
    return ribon::boot::BootArch::X86_64;
}

bool PrepareDirectHighEntry(const ribon::boot::LoadedKernelImage& image, KernelJumpRequest& request) {
    if (image.high_entry_vaddr == 0 || image.high_entry_load_addr == 0 ||
        image.linked_vaddr_end <= image.high_entry_vaddr ||
        image.linked_paddr_start == 0 ||
        image.high_entry_load_addr < image.linked_paddr_start) {
        return false;
    }

    const UINT64 high_start = AlignDown(image.high_entry_vaddr, kLarge2M);
    const UINT64 high_end = AlignUp(image.linked_vaddr_end, kLarge2M);
    const UINT64 high_load =
        AlignDown(image.phys_start + (image.high_entry_load_addr - image.linked_paddr_start),
                  kLarge2M);
    if (high_end <= high_start ||
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

    UINT64 *pml4 = tables;
    UINT64 *low_pdpt = tables + kEntries;
    UINT64 *low_pd0 = tables + (kEntries * 2);
    UINT64 *low_pd1 = tables + (kEntries * 3);
    UINT64 *low_pd2 = tables + (kEntries * 4);
    UINT64 *low_pd3 = tables + (kEntries * 5);
    UINT64 *high_pdpt = tables + (kEntries * 6);
    UINT64 *high_pd = tables + (kEntries * 7);
    UINT64 low_pd_phys[4] = {
        table_phys + kPageSize * 2,
        table_phys + kPageSize * 3,
        table_phys + kPageSize * 4,
        table_phys + kPageSize * 5,
    };

    pml4[0] = TableDesc(table_phys + kPageSize);
    for (UINT32 gb = 0; gb < 4; ++gb) {
        low_pdpt[gb] = TableDesc(low_pd_phys[gb]);
        UINT64 *pd = gb == 0 ? low_pd0 : gb == 1 ? low_pd1 : gb == 2 ? low_pd2 : low_pd3;
        for (UINT32 i = 0; i < kEntries; ++i) {
            pd[i] =
                LargeDesc(static_cast<UINT64>(gb) * kLarge1G +
                          static_cast<UINT64>(i) * kLarge2M);
        }
    }

    const UINT32 pml4_index = static_cast<UINT32>((high_start >> 39) & 0x1FFu);
    const UINT32 pdpt_index = static_cast<UINT32>((high_start >> 30) & 0x1FFu);
    UINT32 pd_index = static_cast<UINT32>((high_start >> 21) & 0x1FFu);
    pml4[pml4_index] = TableDesc(table_phys + kPageSize * 6);
    high_pdpt[pdpt_index] = TableDesc(table_phys + kPageSize * 7);
    for (UINT64 va = high_start; va < high_end; va += kLarge2M) {
        if (pd_index >= kEntries) {
            return false;
        }
        high_pd[pd_index++] = LargeDesc(high_load + (va - high_start));
    }

    request.high_entry = image.high_entry_vaddr;
    request.high_entry_load = high_load;
    request.high_vaddr_start = high_start;
    request.high_vaddr_end = high_end;
    request.high_load_start = high_load;
    request.high_load_end = high_load + (high_end - high_start);
    request.arch_bootstrap0 = table_phys;
    request.entry_flags = kKernelEntryFlagEnteredHigh | kKernelEntryFlagRibonDirectHigh;
    return true;
}

[[noreturn]] void JumpToKernel(const KernelJumpRequest& request) {
    if (request.direct_high_enabled && request.arch_bootstrap0 != 0 && request.high_entry != 0) {
        asm volatile(
            "cld\n"
            "mov %0, %%cr3\n"
            "mov %1, %%rdi\n"
            "mov %2, %%rsi\n"
            "jmpq *%3\n"
            :
            : "r"(request.arch_bootstrap0), "r"(request.arg),
              "r"(static_cast<UINT64>(request.entry_flags)), "r"(request.high_entry)
            : "rdi", "rsi", "memory");
    }

    asm volatile(
        "cld\n"
        "mov %0, %%rdi\n"
        "xor %%rsi, %%rsi\n"
        "jmpq *%1\n"
        :
        : "r"(request.arg), "r"(request.fallback_entry)
        : "rdi", "rsi", "memory");

    __builtin_unreachable();
}

} // namespace ribon::arch
