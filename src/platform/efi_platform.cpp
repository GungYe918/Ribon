#include <Ribon/platform/EfiPlatform.hpp>

#include <Ribon/EfiContext.hpp>
#include <Ribon/File.hpp>
#include <Ribon/Memory.hpp>
#include <Ribon/base/Utf16String.hpp>

namespace ribon::platform {

BootContext CaptureBootContext() {
    BootContext context{};
    context.image_handle = ribon::getImageHandle();
    context.system_table = ribon::getST();
    context.boot_services = ribon::getBS();
    context.gop = ribon::getGop();
    context.file_root = ribon::getFileRoot();
    return context;
}

bool LoadFileToPool(const char* ascii_path, ribon::boot::LoadedFileBuffer& out) {
    out = {};
    if (!ascii_path) {
        return false;
    }

    ribon::str::Utf16String path16(ascii_path);
    EFI_FILE_PROTOCOL* file = ribon::IO::openFile(path16.c_str(), EFI_FILE_MODE_READ);
    if (!file) {
        return false;
    }

    const UINT64 file_size = ribon::IO::fileSize(file);
    if (file_size == 0) {
        file->Close(file);
        return false;
    }

    void* buffer = nullptr;
    if (EFI_ERROR(ribon::mem::AllocatePool(EfiLoaderData, static_cast<UINTN>(file_size), &buffer))) {
        file->Close(file);
        return false;
    }

    UINTN read_size = 0;
    if (!ribon::IO::readFile(file, buffer, static_cast<UINTN>(file_size), &read_size) || read_size != file_size) {
        file->Close(file);
        ribon::mem::FreePool(buffer);
        return false;
    }

    file->Close(file);
    out.data = buffer;
    out.size = read_size;
    return true;
}

void ReleaseLoadedFile(ribon::boot::LoadedFileBuffer& file) {
    if (file.data) {
        ribon::mem::FreePool(file.data);
    }
    file = {};
}

bool AllocateMemoryMapSnapshot(MemoryMapSnapshot& out) {
    out = {};

    EFI_BOOT_SERVICES* bs = ribon::getBS();
    if (!bs) {
        return false;
    }

    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_version = 0;
    EFI_STATUS st = bs->GetMemoryMap(&map_size, nullptr, &map_key, &desc_size, &desc_version);
    if (st != EFI_BUFFER_TOO_SMALL) {
        return false;
    }

    map_size += desc_size * 16;

    EFI_MEMORY_DESCRIPTOR* buffer = nullptr;
    st = ribon::mem::AllocatePool(EfiLoaderData, map_size, reinterpret_cast<void**>(&buffer));
    if (EFI_ERROR(st) || !buffer) {
        return false;
    }

    out.descriptors = buffer;
    out.buffer_size = map_size;
    out.descriptor_size = desc_size;
    out.descriptor_version = desc_version;
    return true;
}

bool RefreshMemoryMapSnapshot(MemoryMapSnapshot& snapshot) {
    EFI_BOOT_SERVICES* bs = ribon::getBS();
    if (!bs || !snapshot.descriptors || snapshot.buffer_size == 0) {
        return false;
    }

    UINTN map_size = snapshot.buffer_size;
    EFI_STATUS st = bs->GetMemoryMap(
        &map_size,
        snapshot.descriptors,
        &snapshot.map_key,
        &snapshot.descriptor_size,
        &snapshot.descriptor_version
    );
    if (EFI_ERROR(st)) {
        snapshot.map_size = 0;
        return false;
    }

    snapshot.map_size = map_size;
    return true;
}

bool ExitBootServicesWithRetry(const BootContext& context, MemoryMapSnapshot& snapshot) {
    if (!context.boot_services || !context.image_handle) {
        return false;
    }

    if (!RefreshMemoryMapSnapshot(snapshot)) {
        return false;
    }

    EFI_STATUS st = context.boot_services->ExitBootServices(context.image_handle, snapshot.map_key);
    if (!EFI_ERROR(st)) {
        return true;
    }

    if (!RefreshMemoryMapSnapshot(snapshot)) {
        return false;
    }

    st = context.boot_services->ExitBootServices(context.image_handle, snapshot.map_key);
    return !EFI_ERROR(st);
}

void ReleaseMemoryMapSnapshot(MemoryMapSnapshot& snapshot) {
    if (snapshot.descriptors && ribon::getBS()) {
        ribon::mem::FreePool(snapshot.descriptors);
    }
    snapshot = {};
}

} // namespace ribon::platform
