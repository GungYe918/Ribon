# src/CMakeLists.txt

add_library(ribon_core STATIC
    core/log.cpp
    core/memory.cpp
    core/utf16_string.cpp
)

add_library(ribon_rt STATIC
    runtime/init.cpp
)

add_library(ribon_elf STATIC
    elf/elf_loader.cpp
)

add_library(ribon_efi STATIC
    efi/efi.cpp
)

add_library(ribon_multiboot STATIC
    multiboot/multiboot_loader.cpp
)

add_library(ribon_fiasco STATIC
    fiasco/fiasco_loader.cpp
)

add_library(ribon_guids STATIC
    guids/guids.cpp
)

add_library(ribon_io STATIC
    io/print.cpp
    io/console.cpp
)
