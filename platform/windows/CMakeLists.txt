cmake_minimum_required(VERSION 3.14)
project(AeroSyncWindows CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Add shared C++ core static library
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../core" "${CMAKE_CURRENT_BINARY_DIR}/core_build")

add_executable(AeroSync WIN32
    src/main.cpp
    src/app_window.cpp
)

target_include_directories(AeroSync PRIVATE
    include
    "${CMAKE_CURRENT_SOURCE_DIR}/../../core/include"
)

target_link_libraries(AeroSync PRIVATE
    aerosync_core
    ws2_32
    mswsock
    iphlpapi
    comctl32
    comdlg32
    shell32
    ole32
    uuid
    gdi32
    user32
)

add_executable(aerosync_daemon
    src/daemon_main.cpp
)

target_include_directories(aerosync_daemon PRIVATE
    include
    "${CMAKE_CURRENT_SOURCE_DIR}/../../core/include"
)

target_link_libraries(aerosync_daemon PRIVATE
    aerosync_core
    ws2_32
    mswsock
    iphlpapi
    shell32
    ole32
    uuid
)
