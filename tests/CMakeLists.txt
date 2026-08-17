cmake_minimum_required(VERSION 3.14)
project(AeroSyncTests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../core" "${CMAKE_CURRENT_BINARY_DIR}/core_build")

# 1. Core Engine Unit Test Executable
add_executable(test_core_engine
    test_core_engine.cpp
)

target_include_directories(test_core_engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/include"
)

target_link_libraries(test_core_engine PRIVATE
    aerosync_core
)

if(WIN32)
    target_link_libraries(test_core_engine PRIVATE ws2_32 mswsock iphlpapi)
endif()

# 2. High-Throughput Synthetic N-GB Benchmark Executable
add_executable(aerosync_benchmark
    test_benchmark_throughput.cpp
)

target_include_directories(aerosync_benchmark PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/include"
)

target_link_libraries(aerosync_benchmark PRIVATE
    aerosync_core
)

if(WIN32)
    target_link_libraries(aerosync_benchmark PRIVATE ws2_32 mswsock iphlpapi)
endif()
