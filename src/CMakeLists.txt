add_library(ribon_core STATIC
    core/log.cpp
    core/utf16_string.cpp
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

add_library(ribon_io STATIC
    io/print.cpp
)

add_library(ribon_gfx STATIC
    gfx/framebuffer.cpp
    gfx/draw.cpp
    gfx/screen.cpp
    gfx/image.cpp
    gfx/doublebuffer.cpp
)

add_library(ribon_ui STATIC
    ui/widget.cpp
    ui/focus.cpp
    ui/render.cpp
    ui/consoleoverlay.cpp
)

add_library(ribon_platform_efi STATIC
    core/eficontext.cpp
    platform/efi_platform.cpp
)

add_library(ribon_image_elf STATIC
    image/elf_image.cpp
)

add_library(ribon_handoff_lbpb STATIC
    handoff/lbpb_builder.cpp
)

add_library(ribon_boot_core STATIC
    boot/boot_core.cpp
)

add_library(ribon_policy_gui STATIC
    policy/gui_boot_policy.cpp
)

add_library(ribon_policy_autoboot STATIC
    policy/autoboot_policy.cpp
)

add_library(ribon_cli_stubs STATIC
    stubs/cli_visual_stubs.cpp
)

target_link_libraries(ribon_console PUBLIC ribon_core ribon_platform_efi ribon_rt)
target_link_libraries(ribon_io PUBLIC ribon_core ribon_console ribon_platform_efi ribon_rt)
target_link_libraries(ribon_gfx PUBLIC ribon_core ribon_io ribon_platform_efi ribon_rt)
target_link_libraries(ribon_ui PUBLIC ribon_core ribon_console ribon_gfx ribon_platform_efi ribon_rt)
target_link_libraries(ribon_platform_efi PUBLIC ribon_core ribon_guids ribon_rt)
target_link_libraries(ribon_image_elf PUBLIC ribon_core ribon_platform_efi ribon_rt)
target_link_libraries(ribon_handoff_lbpb PUBLIC ribon_core ribon_platform_efi ribon_rt ribon_guids loaderPkg)
target_link_libraries(ribon_boot_core PUBLIC
    ribon_core
    ribon_platform_efi
    ribon_image_elf
    ribon_handoff_lbpb
    ribon_io
    ribon_console
    ribon_rt
    loaderPkg
)
target_link_libraries(ribon_policy_gui PUBLIC ribon_core)
target_link_libraries(ribon_policy_autoboot PUBLIC ribon_core)
target_link_libraries(ribon_cli_stubs PUBLIC ribon_core)
