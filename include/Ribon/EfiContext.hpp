// Ribon/Init.hpp
#pragma once

extern "C" {
    #include <Uefi.h>
    #include <Protocol/GraphicsOutput.h>
    #include <Protocol/SimpleFileSystem.h>
}

namespace ribon {

    void initialize(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable);

    EFI_SYSTEM_TABLE* getST();
    EFI_BOOT_SERVICES* getBS();
    EFI_GRAPHICS_OUTPUT_PROTOCOL* getGop();
    EFI_HANDLE getImageHandle();
    EFI_FILE_PROTOCOL* getFileRoot();

} // namespace ribon