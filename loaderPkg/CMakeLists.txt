cmake_minimum_required(VERSION 3.20)

message(STATUS "[loaderPkg] Build loader package")

add_library(loaderPkg INTERFACE)

target_include_directories(loaderPkg INTERFACE
    ${CMAKE_SOURCE_DIR}
)
