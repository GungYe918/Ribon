add_library(ribon_arch STATIC
    uefi/entrypoint.cpp
    uefi/gdt.S
    uefi/paging.S
    uefi/switch_to_kernel.S
)

target_include_directories(ribon_arch PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
