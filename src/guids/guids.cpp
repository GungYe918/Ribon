// Guid.cpp

extern "C" {
    #include <Uefi.h> 
    #include <Protocol/GraphicsOutput.h> 
    #include <Protocol/SimpleTextIn.h> 
    #include <Protocol/SimpleTextOut.h> 
    #include <Protocol/SimpleFileSystem.h> 
    #include <Protocol/LoadedImage.h> 
    #include <Guid/FileInfo.h> 
    #include <Guid/FileSystemInfo.h>
    #include <Protocol/DevicePath.h>
    #include <Protocol/DevicePathToText.h>
    #include <Protocol/DevicePathFromText.h>
    #include <Protocol/BlockIo.h>
    #include <Protocol/DiskIo.h>
    #include <Protocol/SimpleNetwork.h>
    #include <Protocol/PciIo.h>
    #include <Protocol/SerialIo.h>
    #include <Protocol/UsbIo.h>
    #include <Protocol/Usb2HostController.h>
    #include <Protocol/Rng.h>

    #include <Guid/Acpi.h>
    #include <Guid/SmBios.h>
    #include <Guid/GlobalVariable.h>
    #include <Guid/DebugImageInfoTable.h>
}

// GOP
extern "C" EFI_GUID gEfiGraphicsOutputProtocolGuid = {
    0x9042a9de, 0x23dc, 0x4a38,
    { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a }
};

// Simple Text Output
extern "C" EFI_GUID gEfiSimpleTextOutProtocolGuid = {
    0x387477c1, 0x69c7, 0x11d2,
    { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b }
};

// Simple Text Input
extern "C" EFI_GUID gEfiSimpleTextInProtocolGuid = {
    0x387477c1, 0x69c7, 0x11d2,
    { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3c }
};

// Loaded Image Protocol
extern "C" EFI_GUID gEfiLoadedImageProtocolGuid = {
    0x5b1b31a1, 0x9562, 0x11d2,
    { 0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b }
};

// Simple FileSystem Protocol
extern "C" EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
    0x964e5b22, 0x6459, 0x11d2,
    { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b }
};

// FileInfo GUID
extern "C" EFI_GUID gEfiFileInfoGuid = {
    0x09576e92, 0x6d3f, 0x11d2,
    { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b }
};

// FileSystemInfo GUID
extern "C" EFI_GUID gEfiFileSystemInfoGuid = {
    0x09576e93, 0x6d3f, 0x11d2,
    { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b }
};

// Simple Pointer Protocol
extern "C" EFI_GUID gEfiSimplePointerProtocolGuid =  { 
    0x31878c87, 0xb75, 0x11d5,
    { 0x9a, 0x4f, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } 
};

// Simple Text Input Ex Protocol
extern "C" EFI_GUID gEfiSimpleTextInputExProtocolGuid = { 
    0xdd9e7534, 0x7762, 0x4698,
    { 0x8c, 0x14, 0xf5, 0x85, 0x17, 0xa6, 0x25, 0xaa } 
};

// --------------------
//  Device Path 관련
// --------------------
extern "C" EFI_GUID gEfiDevicePathProtocolGuid          = EFI_DEVICE_PATH_PROTOCOL_GUID;
extern "C" EFI_GUID gEfiDevicePathToTextProtocolGuid    = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;
extern "C" EFI_GUID gEfiDevicePathFromTextProtocolGuid  = EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL_GUID;

// --------------------
//  디스크 / 블록 I/O
// --------------------
extern "C" EFI_GUID gEfiBlockIoProtocolGuid             = EFI_BLOCK_IO_PROTOCOL_GUID;
extern "C" EFI_GUID gEfiDiskIoProtocolGuid              = EFI_DISK_IO_PROTOCOL_GUID;

// --------------------
//  네트워크 / PCI
// --------------------
extern "C" EFI_GUID gEfiSimpleNetworkProtocolGuid       = EFI_SIMPLE_NETWORK_PROTOCOL_GUID;
extern "C" EFI_GUID gEfiPciIoProtocolGuid               = EFI_PCI_IO_PROTOCOL_GUID;

// --------------------
//  시리얼 / USB
// --------------------
extern "C" EFI_GUID gEfiSerialIoProtocolGuid            = EFI_SERIAL_IO_PROTOCOL_GUID;
extern "C" EFI_GUID gEfiUsbIoProtocolGuid               = EFI_USB_IO_PROTOCOL_GUID;
extern "C" EFI_GUID gEfiUsb2HostControllerProtocolGuid  = EFI_USB2_HC_PROTOCOL_GUID;

// --------------------
//  난수 / RNG
// --------------------
extern "C" EFI_GUID gEfiRngProtocolGuid                 = EFI_RNG_PROTOCOL_GUID;

// --------------------
//  시스템 테이블 / ACPI / SMBIOS
// --------------------
extern "C" EFI_GUID gEfiAcpiTableGuid                   = EFI_ACPI_TABLE_GUID;
extern "C" EFI_GUID gEfiAcpi10TableGuid                 = ACPI_10_TABLE_GUID;
extern "C" EFI_GUID gEfiAcpi20TableGuid                 = EFI_ACPI_20_TABLE_GUID;

// --------------------
//  Global Variable / Memory / Debug
// --------------------
extern "C" EFI_GUID gEfiDebugImageInfoTableGuid         = EFI_DEBUG_IMAGE_INFO_TABLE_GUID;
