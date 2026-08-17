cmake_minimum_required(VERSION 3.14)
project(aerosync_core CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_SOURCES
    src/pairing_state_machine.cpp
    src/protocol_serializer.cpp
    src/discovery_engine.cpp
    src/socket_transport.cpp
    src/transfer_engine.cpp
    src/connection_manager.cpp
    src/aerosync_app.cpp
)

add_library(aerosync_core STATIC ${CORE_SOURCES})

target_include_directories(aerosync_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(WIN32)
    target_link_libraries(aerosync_core PUBLIC ws2_32 mswsock iphlpapi)
endif()
