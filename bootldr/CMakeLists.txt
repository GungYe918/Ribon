cmake_minimum_required(VERSION 3.20)

message(STATUS "[bootloader] Build Ribon EFI boot applications")

if(ARCH STREQUAL "x86_64")
    set(RIBON_EFI_MACHINE x64)
    set(RIBON_GUI_OUTPUT BOOTX64)
    set(RIBON_AUTO_OUTPUT RBAUTOX64)
elseif(ARCH STREQUAL "aarch64")
    set(RIBON_EFI_MACHINE arm64)
    set(RIBON_GUI_OUTPUT BOOTAA64)
    set(RIBON_AUTO_OUTPUT RBAUTOAA64)
else()
    message(FATAL_ERROR "Unsupported ARCH for bootldr: ${ARCH}")
endif()

function(ribon_apply_efi_link target)
    target_link_options(${target} PRIVATE
        -fuse-ld=lld
        -nostdlib
        -nostdlib++
        -Wl,/subsystem:efi_application
        -Wl,/entry:EfiMain
        -Wl,/machine:${RIBON_EFI_MACHINE}
        -Wl,/nodefaultlib
        -Wl,/dll
        -Wl,/opt:ref
    )

    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/esp/EFI/BOOT
        SUFFIX ".EFI"
    )
endfunction()

if(RIBON_BUILD_GUI)
    add_executable(RibonGUI
        gui_main.cpp
    )

    target_link_libraries(RibonGUI
        ribon_core
        ribon_guids
        ribon_platform_efi
        ribon_image_elf
        ribon_handoff_lbpb
        ribon_boot_core
        ribon_policy_gui
        ribon_io
        ribon_console
        ribon_gfx
        ribon_ui
        ribon_arch
        ribon_rt
        loaderPkg
    )

    ribon_apply_efi_link(RibonGUI)
    set_target_properties(RibonGUI PROPERTIES OUTPUT_NAME "${RIBON_GUI_OUTPUT}")
endif()

if(RIBON_BUILD_AUTOBOOT)
    add_executable(RibonAutoBoot
        autoboot_main.cpp
    )

    target_link_libraries(RibonAutoBoot
        ribon_core
        ribon_guids
        ribon_platform_efi
        ribon_image_elf
        ribon_handoff_lbpb
        ribon_boot_core
        ribon_policy_autoboot
        ribon_io
        ribon_console
        ribon_cli_stubs
        ribon_arch
        ribon_rt
        loaderPkg
    )

    ribon_apply_efi_link(RibonAutoBoot)
    set_target_properties(RibonAutoBoot PROPERTIES OUTPUT_NAME "${RIBON_AUTO_OUTPUT}")
endif()
