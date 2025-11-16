# root cmake
cmake_minimum_required(VERSION 3.20)

project(Ribon LANGUAGES C CXX ASM)

# ----------------------------------------------------
# 0) Architecture selection
# ----------------------------------------------------
if(NOT DEFINED ARCH)
    set(ARCH x86_64)
endif()

message(STATUS "Target Architecture: ${ARCH}")

# toolchain 파일은 cmake 호출 시 -DCMAKE_TOOLCHAIN_FILE 로 지정해야 함.


# ----------------------------------------------------
# 1) Build Options
# ----------------------------------------------------
option(BOOTLOADER "Build EFI Bootloader" ON)
message(STATUS "BOOTLOADER mode: ${BOOTLOADER}")

# ----------------------------------------------------
# 2) Common compile options
# ----------------------------------------------------
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-fshort-wchar)
add_definitions(-DUEFI)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/Ribon

    ${CMAKE_SOURCE_DIR}/include/uefi
    ${CMAKE_SOURCE_DIR}/include/uefi/X64
    ${CMAKE_SOURCE_DIR}/include/uefi/Ia32
    ${CMAKE_SOURCE_DIR}/include/uefi/AArch64
    ${CMAKE_SOURCE_DIR}/include/uefi/Arm
    ${CMAKE_SOURCE_DIR}/include/uefi/Ebc
    ${CMAKE_SOURCE_DIR}/include/uefi/RiscV64
)


# ----------------------------------------------------
# 3) Subdirectories: Core Libraries
# ----------------------------------------------------
add_subdirectory(src)
add_subdirectory(arch/${ARCH})
add_subdirectory(boot)

# ----------------------------------------------------
# 4) Bootloader Mode (Delegated to bootldr/)
# ----------------------------------------------------
if(BOOTLOADER)
    message(STATUS "Including EFI Bootloader...")
    add_subdirectory(bootldr)
else()
    message(STATUS "Bootloader disabled: building Ribon core only.")
endif()
