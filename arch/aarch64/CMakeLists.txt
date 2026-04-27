add_library(ribon_arch STATIC
    jump.cpp
)

target_include_directories(ribon_arch PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
