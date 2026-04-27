#pragma once

extern "C" {
#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleFileSystem.h>
}

#include <Ribon/boot/Types.hpp>

namespace ribon::platform {

struct BootContext {
    EFI_HANDLE image_handle = nullptr;
    EFI_SYSTEM_TABLE* system_table = nullptr;
    EFI_BOOT_SERVICES* boot_services = nullptr;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = nullptr;
    EFI_FILE_PROTOCOL* file_root = nullptr;
};

struct MemoryMapSnapshot {
    EFI_MEMORY_DESCRIPTOR* descriptors = nullptr;
    UINTN buffer_size = 0;
    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    UINT32 descriptor_version = 0;
};

BootContext CaptureBootContext();

bool LoadFileToPool(const char* ascii_path, ribon::boot::LoadedFileBuffer& out);
void ReleaseLoadedFile(ribon::boot::LoadedFileBuffer& file);

bool AllocateMemoryMapSnapshot(MemoryMapSnapshot& out);
bool RefreshMemoryMapSnapshot(MemoryMapSnapshot& snapshot);
bool ExitBootServicesWithRetry(const BootContext& context, MemoryMapSnapshot& snapshot);
void ReleaseMemoryMapSnapshot(MemoryMapSnapshot& snapshot);

} // namespace ribon::platform
