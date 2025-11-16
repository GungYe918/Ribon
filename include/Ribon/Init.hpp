#pragma once
#include <Uefi.h>

namespace ribon {

    void initialize(EFI_SYSTEM_TABLE* SystemTable);

    EFI_SYSTEM_TABLE* getST();
    EFI_BOOT_SERVICES* getBS();

} // namespace ribon