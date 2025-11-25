# src/CMakeLists.txt

add_library(ribon_core STATIC
    core/log.cpp
    core/utf16_string.cpp
    core/init.cpp
)

add_library(ribon_guids STATIC
    guids/guids.cpp
)

add_library(ribon_rt STATIC
    runtime/mem.cpp
    runtime/stub.cpp
    runtime/common.cpp
)

add_library(ribon_console STATIC
    console/console.cpp
    console/font.cpp
    console/font_map.cpp
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

add_library(ribon_gfx STATIC
    gfx/framebuffer.cpp
    gfx/draw.cpp
    gfx/screen.cpp
    gfx/image.cpp
    gfx/doublebuffer.cpp
)

add_library(ribon_io STATIC
    io/print.cpp
)

add_library(ribon_ui STATIC
    ui/widget.cpp
    ui/focus.cpp
    ui/render.cpp
    ui/consoleoverlay.cpp
)
