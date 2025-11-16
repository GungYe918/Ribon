extern "C" {
    #include <Uefi.h>
    #include <Protocol/GraphicsOutput.h>
    #include <Protocol/SimpleTextIn.h>
    #include <Protocol/SimpleTextOut.h>
    #include <Protocol/SimpleFileSystem.h>
    #include <Protocol/LoadedImage.h>
    #include <Guid/FileInfo.h>
    #include <Guid/FileSystemInfo.h>
}

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
{   \
    0x9042a9de, 0x23dc, 0x4a38, \
    {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } \
}

// GOP
EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

// Text Output
EFI_GUID gEfiSimpleTextOutProtocolGuid = EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;

// Text Input
EFI_GUID gEfiSimpleTextInProtocolGuid = EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;

// Loaded Image
EFI_GUID gEfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

// Simple FileSystem Protocol
EFI_GUID gEfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

// FileInfo GUID
EFI_GUID gEfiFileInfoGuid = EFI_FILE_INFO_ID;

// FileSystemInfo GUID
EFI_GUID gEfiFileSystemInfoGuid = EFI_FILE_SYSTEM_INFO_ID;
