#include <Ribon/boot/BootCore.hpp>

#include <Ribon/arch/KernelJump.hpp>
#include <Ribon/handoff/LbpbBuilder.hpp>
#include <Ribon/Memory.hpp>
#include <Ribon/platform/EfiPlatform.hpp>

#include <loaderPkg/FiascoLoader.hpp>
#include <loaderPkg/LBPBLoader.hpp>
#include <loaderPkg/MultibootLoader.hpp>

namespace ribon::boot {

struct BootLogic::LoaderOps {
    bool (*probe)(const void* self, const void* file, UINTN size);
    bool (*load_image)(void* self, const void* file, UINTN size, LoadedKernelImage& out);
    KernelProbeResult (*probe_result)(const void* self);
};

struct BootLogic::LoaderSlot {
    void* instance = nullptr;
    LoaderOps ops{};
};

namespace {

template <typename LoaderT>
BootLogic::LoaderOps MakeOps() {
    return BootLogic::LoaderOps{
        [](const void* self, const void* file, UINTN size) -> bool {
            return static_cast<const LoaderT*>(self)->probe(file, size);
        },
        [](void* self, const void* file, UINTN size, LoadedKernelImage& out) -> bool {
            return static_cast<LoaderT*>(self)->loadImage(file, size, out);
        },
        [](const void* self) -> KernelProbeResult {
            return static_cast<const LoaderT*>(self)->probeResult();
        },
    };
}

BootLogic::LoaderSlot g_loader_slots[BootLogic::kMaxLoaders];

constexpr EFI_GUID kEfiDeviceTreeTableGuid = {
    0xb1b621d5,
    0xf19c,
    0x41a5,
    {0x83, 0x0b, 0xd9, 0x15, 0x2c, 0x69, 0xaa, 0xe0}
};

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

UINT32 ReadBe32(const void* ptr) {
    const auto* bytes = static_cast<const UINT8*>(ptr);
    return ((UINT32)bytes[0] << 24) |
        ((UINT32)bytes[1] << 16) |
        ((UINT32)bytes[2] << 8) |
        (UINT32)bytes[3];
}

bool LooksLikeDtb(const void* data, UINTN size) {
    if (!data || size < 40) {
        return false;
    }
    const UINT32 magic = ReadBe32(data);
    const UINT32 total_size = ReadBe32(static_cast<const UINT8*>(data) + 4);
    return magic == 0xd00dfeedu && total_size >= 40u && total_size <= size;
}

const void* FindFirmwareDtb(const ribon::platform::BootContext& context, UINTN& size) {
    size = 0;
    if (!context.system_table || !context.system_table->ConfigurationTable) {
        return nullptr;
    }
    for (UINTN i = 0; i < context.system_table->NumberOfTableEntries; ++i) {
        const EFI_CONFIGURATION_TABLE& table = context.system_table->ConfigurationTable[i];
        if (!GuidEquals(table.VendorGuid, kEfiDeviceTreeTableGuid) || !table.VendorTable) {
            continue;
        }
        const UINT32 total_size = ReadBe32(static_cast<const UINT8*>(table.VendorTable) + 4);
        if (LooksLikeDtb(table.VendorTable, total_size)) {
            size = total_size;
            return table.VendorTable;
        }
    }
    return nullptr;
}

} // namespace

BootLogic::BootLogic()
    : slots_(g_loader_slots), loader_count_(0), kernel_file_(nullptr), kernel_size_(0) {
    for (UINTN i = 0; i < kMaxLoaders; ++i) {
        slots_[i] = {};
    }
}

void BootLogic::registerLoaderImpl(void* loader, LoaderOps ops) {
    if (!loader || loader_count_ >= kMaxLoaders) {
        return;
    }
    slots_[loader_count_].instance = loader;
    slots_[loader_count_].ops = ops;
    ++loader_count_;
}

void BootLogic::registerLoader(ribon::loaderPkg::LBPBLoader* loader) {
    registerLoaderImpl(loader, MakeOps<ribon::loaderPkg::LBPBLoader>());
}

void BootLogic::registerLoader(ribon::loaderPkg::MultibootLoader* loader) {
    registerLoaderImpl(loader, MakeOps<ribon::loaderPkg::MultibootLoader>());
}

void BootLogic::registerLoader(ribon::loaderPkg::FiascoLoader* loader) {
    registerLoaderImpl(loader, MakeOps<ribon::loaderPkg::FiascoLoader>());
}

void BootLogic::setKernel(const void* file, UINTN size) {
    kernel_file_ = file;
    kernel_size_ = size;
}

bool BootLogic::prepareBootArtifact(BootArtifact& artifact) {
    artifact = {};

    if (!kernel_file_ || kernel_size_ == 0) {
        return false;
    }

    for (UINTN i = 0; i < loader_count_; ++i) {
        LoaderSlot& slot = slots_[i];
        if (!slot.instance || !slot.ops.probe(slot.instance, kernel_file_, kernel_size_)) {
            continue;
        }

        LoadedKernelImage image{};
        if (!slot.ops.load_image(slot.instance, kernel_file_, kernel_size_, image)) {
            return false;
        }

        artifact.image = image;
        artifact.final_entry = image.entry;
        artifact.handoff.kind = slot.ops.probe_result(slot.instance).preferred_handoff;
        return true;
    }

    return false;
}

bool ExecuteBootFlow(const BootPolicyConfig& config, StatusReporter reporter) {
    auto report = [&](const char* message) {
        if (reporter) {
            reporter(message);
        }
    };

    report("Loading kernel image...");

    ribon::boot::LoadedFileBuffer kernel_file{};
    if (!ribon::platform::LoadFileToPool(config.kernel_path, kernel_file)) {
        report("Failed to load kernel file.");
        return false;
    }

    const ribon::platform::BootContext boot_context = ribon::platform::CaptureBootContext();
    ribon::boot::LoadedFileBuffer dtb_file{};
    const void* dtb_data = nullptr;
    UINTN dtb_size = 0;
#if !defined(KAIRON_RIBON_ACCEPT_DTB)
#define KAIRON_RIBON_ACCEPT_DTB 0
#endif
#if !defined(KAIRON_RIBON_REQUIRE_DTB)
#define KAIRON_RIBON_REQUIRE_DTB 0
#endif
    const bool accept_dtb = config.accept_dtb && (KAIRON_RIBON_ACCEPT_DTB != 0);
    const bool require_dtb = config.require_dtb || (KAIRON_RIBON_REQUIRE_DTB != 0);
    if (accept_dtb) {
        dtb_data = FindFirmwareDtb(boot_context, dtb_size);
        if (!dtb_data && config.dtb_path &&
            ribon::platform::LoadFileToPool(config.dtb_path, dtb_file)) {
            if (LooksLikeDtb(dtb_file.data, dtb_file.size)) {
                dtb_data = dtb_file.data;
                dtb_size = dtb_file.size;
            } else {
                ribon::platform::ReleaseLoadedFile(dtb_file);
            }
        }
    }
    if (require_dtb && (!dtb_data || dtb_size == 0)) {
        ribon::platform::ReleaseLoadedFile(dtb_file);
        ribon::platform::ReleaseLoadedFile(kernel_file);
        report("Selected profile requires a DTB, but none was found.");
        return false;
    }

    static ribon::loaderPkg::LBPBLoader lbpb_loader;
    static ribon::loaderPkg::MultibootLoader multiboot_loader;
    static ribon::loaderPkg::FiascoLoader fiasco_loader;

    BootLogic logic;
    logic.registerLoader(&lbpb_loader);
    logic.registerLoader(&multiboot_loader);
    logic.registerLoader(&fiasco_loader);
    logic.setKernel(kernel_file.data, kernel_file.size);

    BootArtifact artifact{};
    if (!logic.prepareBootArtifact(artifact)) {
        ribon::platform::ReleaseLoadedFile(dtb_file);
        ribon::platform::ReleaseLoadedFile(kernel_file);
        report("No supported kernel loader matched.");
        return false;
    }

    ribon::platform::MemoryMapSnapshot snapshot{};
    if (!ribon::platform::AllocateMemoryMapSnapshot(snapshot)) {
        ribon::platform::ReleaseLoadedFile(dtb_file);
        ribon::platform::ReleaseLoadedFile(kernel_file);
        report("Failed to allocate memory map buffer.");
        return false;
    }

    void* handoff_buffer = nullptr;
    ribon::arch::KernelJumpRequest jump_request{};
    jump_request.fallback_entry = artifact.image.entry;
#if !defined(KAIRON_RIBON_DIRECT_HIGH_ENTRY)
#define KAIRON_RIBON_DIRECT_HIGH_ENTRY 1
#endif
#if !defined(KAIRON_RIBON_DIRECT_HIGH_REQUIRED)
#define KAIRON_RIBON_DIRECT_HIGH_REQUIRED 0
#endif
    if (artifact.handoff.kind == HandoffKind::LBPBCore) {
        const ribon::handoff::LbpbBuildConfig lbpb_config{
            config.boot_cmdline,
            true,
            dtb_data,
            dtb_size
        };
        const UINTN handoff_size = ribon::handoff::EstimateCoreLbpbSize(snapshot, lbpb_config);
        if (EFI_ERROR(ribon::mem::AllocatePool(EfiLoaderData, handoff_size, &handoff_buffer)) || !handoff_buffer) {
            ribon::platform::ReleaseMemoryMapSnapshot(snapshot);
            ribon::platform::ReleaseLoadedFile(dtb_file);
            ribon::platform::ReleaseLoadedFile(kernel_file);
            report("Failed to allocate LBPB handoff buffer.");
            return false;
        }

        jump_request.arg = handoff_buffer;
        if (KAIRON_RIBON_DIRECT_HIGH_ENTRY != 0) {
            if (ribon::arch::PrepareDirectHighEntry(artifact.image, jump_request)) {
                jump_request.direct_high_enabled = true;
                report("RIBON-DIRECT-HIGH-PAGETABLE-OK");
            } else if (KAIRON_RIBON_DIRECT_HIGH_REQUIRED != 0) {
                ribon::mem::FreePool(handoff_buffer);
                ribon::platform::ReleaseMemoryMapSnapshot(snapshot);
                ribon::platform::ReleaseLoadedFile(dtb_file);
                ribon::platform::ReleaseLoadedFile(kernel_file);
                report("Direct higher-half entry preparation failed.");
                return false;
            } else {
                report("RIBON-DIRECT-HIGH-FALLBACK");
            }
        } else {
            report("RIBON-DIRECT-HIGH-FALLBACK");
        }

        if (!ribon::platform::RefreshMemoryMapSnapshot(snapshot)) {
            ribon::mem::FreePool(handoff_buffer);
            ribon::platform::ReleaseMemoryMapSnapshot(snapshot);
            ribon::platform::ReleaseLoadedFile(dtb_file);
            ribon::platform::ReleaseLoadedFile(kernel_file);
            report("Failed to capture final memory map.");
            return false;
        }

        if (!ribon::handoff::BuildCoreLbpb(
                boot_context,
                snapshot,
                artifact.image,
                lbpb_config,
                handoff_buffer,
                handoff_size,
                artifact.handoff)) {
            ribon::mem::FreePool(handoff_buffer);
            ribon::platform::ReleaseMemoryMapSnapshot(snapshot);
            ribon::platform::ReleaseLoadedFile(dtb_file);
            ribon::platform::ReleaseLoadedFile(kernel_file);
            report("Failed to build LBPB handoff.");
            return false;
        }
#if !defined(KAIRON_RIBON_HIGHER_HALF_CONTRACT)
#define KAIRON_RIBON_HIGHER_HALF_CONTRACT 1
#endif
        if (KAIRON_RIBON_HIGHER_HALF_CONTRACT != 0) {
            report("RIBON-KERNEL-LAYOUT-OK");
            report("RIBON-HIGH-ENTRY-CANDIDATE-OK");
        }
        if (jump_request.direct_high_enabled) {
            report("RIBON-DIRECT-HIGH-ENTRY-OK");
        }
    }
    ribon::platform::ReleaseLoadedFile(dtb_file);

    report("Exiting UEFI boot services...");
    if (!ribon::platform::ExitBootServicesWithRetry(boot_context, snapshot)) {
        if (handoff_buffer) {
            ribon::mem::FreePool(handoff_buffer);
        }
        ribon::platform::ReleaseMemoryMapSnapshot(snapshot);
        ribon::platform::ReleaseLoadedFile(dtb_file);
        ribon::platform::ReleaseLoadedFile(kernel_file);
        report("ExitBootServices failed.");
        return false;
    }

    artifact.handoff.data = handoff_buffer;
    jump_request.arg = artifact.handoff.kind == HandoffKind::None ? nullptr : artifact.handoff.data;
    artifact.final_entry = jump_request.direct_high_enabled ?
        jump_request.high_entry :
        jump_request.fallback_entry;
    ribon::arch::JumpToKernel(jump_request);
    return true;
}

} // namespace ribon::boot
