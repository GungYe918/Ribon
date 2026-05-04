cmake_minimum_required(VERSION 3.20)

project(Ribon LANGUAGES C CXX ASM)

set(RIBON_SUPPORTED_ARCHES x86_64 aarch64)
if(NOT DEFINED ARCH)
    set(ARCH x86_64)
endif()

list(FIND RIBON_SUPPORTED_ARCHES "${ARCH}" RIBON_ARCH_INDEX)
if(RIBON_ARCH_INDEX EQUAL -1)
    message(FATAL_ERROR "Unsupported ARCH='${ARCH}'. Supported values: ${RIBON_SUPPORTED_ARCHES}")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/arch/${ARCH}/CMakeLists.txt")
    message(FATAL_ERROR "Missing architecture module for ARCH='${ARCH}'")
endif()

message(STATUS "Target Architecture: ${ARCH}")

option(BOOTLOADER "Build EFI Bootloader" ON)
option(RIBON_BUILD_GUI "Build GUI EFI bootloader target" ON)
option(RIBON_BUILD_AUTOBOOT "Build CLI autoboot EFI target" ON)
set(KAIRON_RIBON_ACCEPT_DTB "0" CACHE STRING "Accept DTB handoff")
set(KAIRON_RIBON_REQUIRE_DTB "0" CACHE STRING "Require DTB handoff")
set(KAIRON_RIBON_GOP_WIDTH "600" CACHE STRING "Requested GOP framebuffer width")
set(KAIRON_RIBON_GOP_HEIGHT "800" CACHE STRING "Requested GOP framebuffer height")
set(KAIRON_RIBON_HIGHER_HALF_CONTRACT "1" CACHE STRING "Emit higher-half kernel image layout contract")

message(STATUS "BOOTLOADER mode: ${BOOTLOADER}")
message(STATUS "GUI target: ${RIBON_BUILD_GUI}")
message(STATUS "Autoboot target: ${RIBON_BUILD_AUTOBOOT}")

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-fshort-wchar)
add_compile_options(-fno-stack-check)
add_definitions(-DUEFI)
add_compile_definitions(
    KAIRON_RIBON_ACCEPT_DTB=${KAIRON_RIBON_ACCEPT_DTB}
    KAIRON_RIBON_REQUIRE_DTB=${KAIRON_RIBON_REQUIRE_DTB}
    KAIRON_RIBON_GOP_WIDTH=${KAIRON_RIBON_GOP_WIDTH}
    KAIRON_RIBON_GOP_HEIGHT=${KAIRON_RIBON_GOP_HEIGHT}
    KAIRON_RIBON_HIGHER_HALF_CONTRACT=${KAIRON_RIBON_HIGHER_HALF_CONTRACT})

if(ARCH STREQUAL "x86_64")
    add_compile_definitions(RIBON_ARCH_X86_64=1)
elseif(ARCH STREQUAL "aarch64")
    add_compile_definitions(RIBON_ARCH_AARCH64=1)
endif()

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

add_subdirectory(loaderPkg)
add_subdirectory(src)
add_subdirectory(arch/${ARCH})

if(BOOTLOADER)
    message(STATUS "Including EFI Bootloader...")
    add_subdirectory(bootldr)
else()
    message(STATUS "Bootloader disabled: building Ribon core only.")
endif()
