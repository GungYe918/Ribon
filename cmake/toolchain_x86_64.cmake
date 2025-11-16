# toolchain_x86_64.cmake
# UEFI x86_64 (Clang + lld-link)
# UEFI x86_64 (Clang + lld-link)
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)

# Makefile과 동일한 타겟 사용 (wchar_t = 2 bytes)
set(COMMON_FLAGS "-target amd64-windows -ffreestanding -fshort-wchar -mno-red-zone -fno-stack-protector")

set(CMAKE_C_FLAGS   "${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS} -fno-rtti -fno-exceptions -nostdinc++")
set(CMAKE_ASM_FLAGS "${COMMON_FLAGS}")

# lld 사용
set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
