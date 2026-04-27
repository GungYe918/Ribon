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
        ribon::platform::ReleaseLoadedFile(kernel_file);
        report("No supported kernel loader matched.");
        return false;
    }

    ribon::platform::MemoryMapSnapshot snapshot{};
    if (!ribon::platform::AllocateMemoryMapSnapshot(snapshot)) {
        ribon::platform::ReleaseLoadedFile(kernel_file);
        report("Failed to allocate memory map buffer.");
        return false;
    }

    void* handoff_buffer = nullptr;
    if (artifact.handoff.kind == HandoffKind::LBPBCore) {
        const ribon::handoff::LbpbBuildConfig lbpb_config{config.boot_cmdline, true};
        const UINTN handoff_size = ribon::handoff::EstimateCoreLbpbSize(snapshot, lbpb_config);
        if (EFI_ERROR(ribon::mem::AllocatePool(EfiLoaderData, handoff_size, &handoff_buffer)) || !handoff_buffer) {
            ribon::platform::ReleaseMemoryMapSnapshot(snapshot);
            ribon::platform::ReleaseLoadedFile(kernel_file);
            report("Failed to allocate LBPB handoff buffer.");
            return false;
        }

        if (!ribon::platform::RefreshMemoryMapSnapshot(snapshot)) {
            ribon::mem::FreePool(handoff_buffer);
            ribon::platform::ReleaseMemoryMapSnapshot(snapshot);
            ribon::platform::ReleaseLoadedFile(kernel_file);
            report("Failed to capture final memory map.");
            return false;
        }

        if (!ribon::handoff::BuildCoreLbpb(
                ribon::platform::CaptureBootContext(),
                snapshot,
                artifact.image,
                lbpb_config,
                handoff_buffer,
                handoff_size,
                artifact.handoff)) {
            ribon::mem::FreePool(handoff_buffer);
            ribon::platform::ReleaseMemoryMapSnapshot(snapshot);
            ribon::platform::ReleaseLoadedFile(kernel_file);
            report("Failed to build LBPB handoff.");
            return false;
        }
    }

    report("Exiting UEFI boot services...");
    const ribon::platform::BootContext context = ribon::platform::CaptureBootContext();
    if (!ribon::platform::ExitBootServicesWithRetry(context, snapshot)) {
        if (handoff_buffer) {
            ribon::mem::FreePool(handoff_buffer);
        }
        ribon::platform::ReleaseMemoryMapSnapshot(snapshot);
        ribon::platform::ReleaseLoadedFile(kernel_file);
        report("ExitBootServices failed.");
        return false;
    }

    artifact.handoff.data = handoff_buffer;
    artifact.final_entry = artifact.image.entry;
    ribon::arch::JumpToKernel(
        artifact.final_entry,
        artifact.handoff.kind == HandoffKind::None ? nullptr : artifact.handoff.data);
    return true;
}

} // namespace ribon::boot
