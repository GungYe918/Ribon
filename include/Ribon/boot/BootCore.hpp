#pragma once

extern "C" {
#include <Uefi.h>
}

#include <Ribon/boot/Types.hpp>

namespace ribon::loaderPkg {
class LBPBLoader;
class MultibootLoader;
class FiascoLoader;
}

namespace ribon::boot {

using StatusReporter = void (*)(const char* message);

class BootLogic {
public:
    struct LoaderOps;
    struct LoaderSlot;
    static constexpr UINTN kMaxLoaders = 16;

    BootLogic();

    void registerLoader(ribon::loaderPkg::LBPBLoader* loader);
    void registerLoader(ribon::loaderPkg::MultibootLoader* loader);
    void registerLoader(ribon::loaderPkg::FiascoLoader* loader);

    void setKernel(const void* file, UINTN size);
    bool prepareBootArtifact(BootArtifact& artifact);

private:
    void registerLoaderImpl(void* loader, LoaderOps ops);
    LoaderSlot* slots_;
    UINTN loader_count_;
    const void* kernel_file_;
    UINTN kernel_size_;
};

bool ExecuteBootFlow(const BootPolicyConfig& config, StatusReporter reporter);

} // namespace ribon::boot
