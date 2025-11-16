#include <Ribon/Init.hpp>

namespace ribon {

    static EFI_SYSTEM_TABLE* gST = nullptr;
    static EFI_BOOT_SERVICES* gBS = nullptr;

    void initialize(EFI_SYSTEM_TABLE* SystemTable) {
        gST = SystemTable;
        gBS = SystemTable->BootServices;
    }   

    EFI_SYSTEM_TABLE* getST() { return gST; }
    EFI_BOOT_SERVICES* getBS() { return gBS; }

} // namespace ribon