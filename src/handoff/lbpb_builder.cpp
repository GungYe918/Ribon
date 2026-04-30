#include <Ribon/handoff/LbpbBuilder.hpp>

extern "C" {
#include <Guid/Acpi.h>
}

#include <Ribon/EfiContext.hpp>
#include <Ribon/Memory.hpp>
#include <loaderPkg/leyn_bpb.h>

namespace {

constexpr UINTN kMaxCoreSections = 8;

UINTN AlignUp(UINTN value, UINTN alignment) {
    if (alignment == 0) {
        return value;
    }
    const UINTN mask = alignment - 1;
    return (value + mask) & ~mask;
}

bool GuidEquals(const EFI_GUID& lhs, const EFI_GUID& rhs) {
    return lhs.Data1 == rhs.Data1 &&
        lhs.Data2 == rhs.Data2 &&
        lhs.Data3 == rhs.Data3 &&
        lhs.Data4[0] == rhs.Data4[0] &&
        lhs.Data4[1] == rhs.Data4[1] &&
        lhs.Data4[2] == rhs.Data4[2] &&
        lhs.Data4[3] == rhs.Data4[3] &&
        lhs.Data4[4] == rhs.Data4[4] &&
        lhs.Data4[5] == rhs.Data4[5] &&
        lhs.Data4[6] == rhs.Data4[6] &&
        lhs.Data4[7] == rhs.Data4[7];
}

UINT32 NormalizeEfiMemoryType(UINT32 type) {
    switch (type) {
    case EfiConventionalMemory:
    case EfiBootServicesCode:
    case EfiBootServicesData:
    case EfiLoaderCode:
    case EfiLoaderData:
        return 1;
    case EfiACPIReclaimMemory:
        return 3;
    case EfiACPIMemoryNVS:
        return 4;
    case EfiUnusableMemory:
        return 5;
    default:
        return 2;
    }
}

template <typename T>
bool WriteObject(UINT8* buffer, UINTN capacity, UINTN& cursor, const T& object, UINTN alignment) {
    const UINTN aligned = AlignUp(cursor, alignment);
    if (aligned + sizeof(T) > capacity) {
        return false;
    }
    ribon::mem::Memcpy(buffer + aligned, &object, sizeof(T));
    cursor = aligned + sizeof(T);
    return true;
}

bool WriteBytes(UINT8* buffer, UINTN capacity, UINTN& cursor, const void* data, UINTN size, UINTN alignment) {
    const UINTN aligned = AlignUp(cursor, alignment);
    if (aligned + size > capacity) {
        return false;
    }
    if (size > 0) {
        ribon::mem::Memcpy(buffer + aligned, data, size);
    }
    cursor = aligned + size;
    return true;
}

} // namespace

namespace ribon::handoff {

UINTN EstimateCoreLbpbSize(const ribon::platform::MemoryMapSnapshot& snapshot, const LbpbBuildConfig& config) {
    const UINTN descriptor_size = snapshot.descriptor_size == 0 ? sizeof(EFI_MEMORY_DESCRIPTOR) : snapshot.descriptor_size;
    const UINTN map_size = snapshot.map_size == 0 ? snapshot.buffer_size : snapshot.map_size;

    UINTN total =
        AlignUp(sizeof(leyn_bpb_header), LEYN_BPB_SECTION_TABLE_ALIGN) +
        kMaxCoreSections * sizeof(leyn_bpb_section) +
        LEYN_BPB_SECTION_PAYLOAD_ALIGN;

    total += AlignUp((map_size / descriptor_size) * sizeof(leyn_bpb_mmap_entry), 64);
    total += AlignUp(sizeof(leyn_bpb_efi_mmap_meta) + map_size, 64);
    total += AlignUp(sizeof(leyn_bpb_fb_info), 64);
    total += AlignUp(sizeof(UINT64), 64);

    if (config.boot_cmdline) {
        UINTN cmd_len = 0;
        while (config.boot_cmdline[cmd_len] != '\0') {
            ++cmd_len;
        }
        total += AlignUp(cmd_len + 1, 64);
    }
    if (config.device_tree && config.device_tree_size > 0) {
        total += AlignUp(config.device_tree_size, 64);
    }

    total += 1024;
    return total;
}

bool BuildCoreLbpb(
    const ribon::platform::BootContext& context,
    const ribon::platform::MemoryMapSnapshot& snapshot,
    const ribon::boot::LoadedKernelImage& image,
    const LbpbBuildConfig& config,
    void* buffer,
    UINTN capacity,
    ribon::boot::HandoffBlob& out
) {
    out = {};
    if (!buffer || !snapshot.descriptors || snapshot.map_size == 0 || snapshot.descriptor_size == 0) {
        return false;
    }

    UINT8* bytes = static_cast<UINT8*>(buffer);
    ribon::mem::Memset(bytes, 0, capacity);

    auto* header = reinterpret_cast<leyn_bpb_header*>(bytes);
    auto* sections = reinterpret_cast<leyn_bpb_section*>(
        bytes + AlignUp(sizeof(leyn_bpb_header), LEYN_BPB_SECTION_TABLE_ALIGN)
    );

    UINTN cursor =
        AlignUp(sizeof(leyn_bpb_header), LEYN_BPB_SECTION_TABLE_ALIGN) +
        kMaxCoreSections * sizeof(leyn_bpb_section);
    cursor = AlignUp(cursor, LEYN_BPB_SECTION_PAYLOAD_ALIGN);

    UINT32 section_count = 0;

    auto append_section = [&](UINT32 type, UINT16 flags, UINT16 alignment, const void* payload, UINTN size, UINT64 aux_data) -> bool {
        if (section_count >= kMaxCoreSections) {
            return false;
        }

        const UINTN aligned = AlignUp(cursor, alignment);
        if (aligned + size > capacity) {
            return false;
        }

        if (size > 0 && payload) {
            ribon::mem::Memcpy(bytes + aligned, payload, size);
        }

        sections[section_count].type = type;
        sections[section_count].flags = flags;
        sections[section_count].alignment = alignment;
        sections[section_count].reserved = 0;
        sections[section_count].payload_offset = aligned;
        sections[section_count].payload_size = size;
        sections[section_count].uncompressed_size = size;
        sections[section_count].aux_data = aux_data;

        ++section_count;
        cursor = aligned + size;
        return true;
    };

    bool has_fb_info = false;
    bool has_acpi = false;

    const UINTN entry_count = snapshot.map_size / snapshot.descriptor_size;
    const UINTN mmap_bytes = entry_count * sizeof(leyn_bpb_mmap_entry);
    const UINTN mmap_section_offset = AlignUp(cursor, 64);
    if (mmap_section_offset + mmap_bytes > capacity) {
        return false;
    }
    auto* normalized_map = reinterpret_cast<leyn_bpb_mmap_entry*>(bytes + mmap_section_offset);
    for (UINTN i = 0; i < entry_count; ++i) {
        const auto* desc = reinterpret_cast<const EFI_MEMORY_DESCRIPTOR*>(
            reinterpret_cast<const UINT8*>(snapshot.descriptors) + i * snapshot.descriptor_size
        );
        normalized_map[i].base_addr = desc->PhysicalStart;
        normalized_map[i].length = desc->NumberOfPages * 4096ULL;
        normalized_map[i].type = NormalizeEfiMemoryType(desc->Type);
        normalized_map[i].flags = 0;
    }
    sections[section_count].type = LEYN_BPB_SEC_MEMORY_MAP;
    sections[section_count].flags = LEYN_BPB_SEC_FLAG_CORE | LEYN_BPB_SEC_FLAG_SRC_EFI | LEYN_BPB_SEC_FLAG_EARLY_MAP;
    sections[section_count].alignment = 64;
    sections[section_count].reserved = 0;
    sections[section_count].payload_offset = mmap_section_offset;
    sections[section_count].payload_size = mmap_bytes;
    sections[section_count].uncompressed_size = mmap_bytes;
    sections[section_count].aux_data = entry_count;
    ++section_count;
    cursor = mmap_section_offset + mmap_bytes;

    const UINTN efi_map_payload_size = sizeof(leyn_bpb_efi_mmap_meta) + snapshot.map_size;
    const UINTN efi_map_section_offset = AlignUp(cursor, 64);
    if (efi_map_section_offset + efi_map_payload_size > capacity) {
        return false;
    }
    auto* efi_meta = reinterpret_cast<leyn_bpb_efi_mmap_meta*>(bytes + efi_map_section_offset);
    efi_meta->descriptor_size = snapshot.descriptor_size;
    efi_meta->descriptor_version = snapshot.descriptor_version;
    ribon::mem::Memcpy(bytes + efi_map_section_offset + sizeof(leyn_bpb_efi_mmap_meta), snapshot.descriptors, snapshot.map_size);
    sections[section_count].type = LEYN_BPB_SEC_EFI_MEMORY_MAP;
    sections[section_count].flags = LEYN_BPB_SEC_FLAG_CORE | LEYN_BPB_SEC_FLAG_SRC_EFI | LEYN_BPB_SEC_FLAG_EARLY_MAP;
    sections[section_count].alignment = 64;
    sections[section_count].reserved = 0;
    sections[section_count].payload_offset = efi_map_section_offset;
    sections[section_count].payload_size = efi_map_payload_size;
    sections[section_count].uncompressed_size = efi_map_payload_size;
    sections[section_count].aux_data = 0;
    ++section_count;
    cursor = efi_map_section_offset + efi_map_payload_size;

    if (context.gop && context.gop->Mode && context.boot_services) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* mode_info = nullptr;
        UINTN mode_info_size = 0;
        EFI_STATUS mode_status = context.gop->QueryMode(
            context.gop,
            context.gop->Mode->Mode,
            &mode_info_size,
            &mode_info
        );

        const auto* gop_mode = context.gop->Mode;
        const auto* info = (!EFI_ERROR(mode_status) && mode_info && mode_info_size >= sizeof(*mode_info))
            ? mode_info
            : gop_mode->Info;

        if (info) {
        leyn_bpb_fb_info fb{};
        fb.framebuffer_addr = gop_mode->FrameBufferBase;
        fb.framebuffer_pitch = info->PixelsPerScanLine * 4;
        fb.framebuffer_width = info->HorizontalResolution;
        fb.framebuffer_height = info->VerticalResolution;
        fb.framebuffer_bpp = 32;
        fb.framebuffer_type = LEYN_BPB_FB_TYPE_RGB;
        fb.backend = LEYN_BPB_FB_BACKEND_UEFI_GOP;

        switch (info->PixelFormat) {
        case PixelBlueGreenRedReserved8BitPerColor:
            fb.color.rgb.red_position = 16;
            fb.color.rgb.red_mask_size = 8;
            fb.color.rgb.green_position = 8;
            fb.color.rgb.green_mask_size = 8;
            fb.color.rgb.blue_position = 0;
            fb.color.rgb.blue_mask_size = 8;
            break;
        case PixelRedGreenBlueReserved8BitPerColor:
            fb.color.rgb.red_position = 0;
            fb.color.rgb.red_mask_size = 8;
            fb.color.rgb.green_position = 8;
            fb.color.rgb.green_mask_size = 8;
            fb.color.rgb.blue_position = 16;
            fb.color.rgb.blue_mask_size = 8;
            break;
        default:
            fb.color.rgb.red_position = 0;
            fb.color.rgb.red_mask_size = 8;
            fb.color.rgb.green_position = 8;
            fb.color.rgb.green_mask_size = 8;
            fb.color.rgb.blue_position = 16;
            fb.color.rgb.blue_mask_size = 8;
            break;
        }

        if (!append_section(
                LEYN_BPB_SEC_FRAMEBUFFER_INFO,
                LEYN_BPB_SEC_FLAG_CORE | LEYN_BPB_SEC_FLAG_SRC_EFI | LEYN_BPB_SEC_FLAG_HW_RELATED,
                64,
                &fb,
                sizeof(fb),
                0)) {
            if (mode_info) {
                ribon::mem::FreePool(mode_info);
            }
            return false;
        }
        has_fb_info = true;
        }

        if (mode_info) {
            ribon::mem::FreePool(mode_info);
        }
    }

    if (context.system_table && context.system_table->ConfigurationTable) {
        void* rsdp = nullptr;
        UINT32 rsdp_type = 0;
        for (UINTN i = 0; i < context.system_table->NumberOfTableEntries; ++i) {
            const EFI_CONFIGURATION_TABLE& table = context.system_table->ConfigurationTable[i];
            if (GuidEquals(table.VendorGuid, gEfiAcpi20TableGuid)) {
                rsdp = table.VendorTable;
                rsdp_type = LEYN_BPB_SEC_ACPI_RSDP_V2;
                break;
            }
            if (!rsdp && GuidEquals(table.VendorGuid, gEfiAcpi10TableGuid)) {
                rsdp = table.VendorTable;
                rsdp_type = LEYN_BPB_SEC_ACPI_RSDP_V1;
            }
        }
        if (rsdp && rsdp_type != 0) {
            if (!append_section(
                    rsdp_type,
                    LEYN_BPB_SEC_FLAG_OPTIONAL | LEYN_BPB_SEC_FLAG_SRC_EFI | LEYN_BPB_SEC_FLAG_HW_RELATED,
                    16,
                    rsdp,
                    36,
                    0)) {
                return false;
            }
            has_acpi = true;
        }
    }

    if (config.device_tree && config.device_tree_size > 0) {
        if (!append_section(
                LEYN_BPB_SEC_DEVICE_TREE,
                LEYN_BPB_SEC_FLAG_OPTIONAL |
                    LEYN_BPB_SEC_FLAG_SRC_FDT |
                    LEYN_BPB_SEC_FLAG_HW_RELATED |
                    LEYN_BPB_SEC_FLAG_EARLY_MAP,
                64,
                config.device_tree,
                config.device_tree_size,
                0)) {
            return false;
        }
    }

    const UINT64 image_load_base = image.phys_start;
    if (!append_section(
            LEYN_BPB_SEC_IMAGE_LOAD_BASE,
            LEYN_BPB_SEC_FLAG_CORE | LEYN_BPB_SEC_FLAG_EARLY_MAP,
            16,
            &image_load_base,
            sizeof(image_load_base),
            0)) {
        return false;
    }

    if (config.boot_cmdline) {
        UINTN cmd_len = 0;
        while (config.boot_cmdline[cmd_len] != '\0') {
            ++cmd_len;
        }
        if (!append_section(
                LEYN_BPB_SEC_BOOT_CMDLINE,
                LEYN_BPB_SEC_FLAG_OPTIONAL | LEYN_BPB_SEC_FLAG_HOT,
                16,
                config.boot_cmdline,
                cmd_len + 1,
                0)) {
            return false;
        }
    }

    header->magic = LEYN_BPB_MAGIC;
    header->version_major = LEYN_BPB_VERSION_MAJOR;
    header->version_minor = LEYN_BPB_VERSION_MINOR;
    header->arch = image.arch == ribon::boot::BootArch::AArch64 ? LEYN_BPB_ARCH_AARCH64 : LEYN_BPB_ARCH_X86_64;
    header->arch_reserved = 0;
    header->header_size = sizeof(leyn_bpb_header);
    header->total_size = cursor;
    header->section_count = section_count;
    header->flags =
        LEYN_BPB_FLAG_LITTLE_ENDIAN |
        LEYN_BPB_FLAG_64BIT_ADDR |
        LEYN_BPB_FLAG_REQUIRE_MEMMAP |
        LEYN_BPB_FLAG_EFI_BOOT;

    if (config.core_only) {
        header->flags |= LEYN_BPB_FLAG_CORE_ONLY;
    }
    if (has_fb_info) {
        header->flags |= LEYN_BPB_FLAG_HAS_FB_INFO;
    }
    if (has_acpi) {
        header->flags |= LEYN_BPB_FLAG_HAS_ACPI;
    }

    if (image.load_policy == ribon::boot::LoadAddressPolicy::UsePaddrWhenAvailable) {
        header->flags |= LEYN_BPB_FLAG_ENTRY_PHYS_VALID;
    } else {
        header->flags |= LEYN_BPB_FLAG_ENTRY_VIRT_VALID;
    }

    header->kernel_entry_vaddr = image.entry;
    header->reserved[0] = reinterpret_cast<UINTN>(context.system_table);
    header->reserved[1] = reinterpret_cast<UINTN>(context.image_handle);

    out.data = buffer;
    out.size = cursor;
    out.kind = ribon::boot::HandoffKind::LBPBCore;
    return true;
}

} // namespace ribon::handoff
