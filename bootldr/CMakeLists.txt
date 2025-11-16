# bootldr/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

message(STATUS "[bootldr] Build Ribon EFI Bootloader")

add_executable(RibonEFI
    main.cpp
)

# lld-link (MSVC 스타일)로 전달되도록 -Wl,와 함께 / 또는 - 사용
target_link_options(RibonEFI PRIVATE
    -fuse-ld=lld
    -nostdlib
    -nostdlib++

    # --- lld-link 옵션들 ---
    -Wl,/subsystem:efi_application
    -Wl,/entry:EfiMain        
    -Wl,/machine:x64
    -Wl,/nodefaultlib
    -Wl,/dll
    -Wl,/opt:ref
)

target_link_libraries(RibonEFI
    ribon_core
    ribon_elf
    ribon_efi
    ribon_multiboot
    ribon_fiasco
    ribon_guids
    ribon_rt
    ribon_arch
)

set_target_properties(RibonEFI PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/esp/EFI/BOOT
    OUTPUT_NAME "BOOTX64"
    SUFFIX ".EFI"
)
