cmake_minimum_required(VERSION 3.22.1)
project(aerosync_jni CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the shared C++ core static library
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../core" "${CMAKE_CURRENT_BINARY_DIR}/core_build")

add_library(aerosync_jni SHARED
    native_bridge.cpp
)

target_include_directories(aerosync_jni PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../core/include"
)

target_link_libraries(aerosync_jni
    aerosync_core
    log
)
