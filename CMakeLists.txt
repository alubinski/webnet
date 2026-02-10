cmake_minimum_required(VERSION 3.20)

project(NetLib
    VERSION 0.1
    LANGUAGES CXX
)

# ---- C++ Standard ----
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# Source Files
# ============================================================

# Core sources
set(NETLIB_CORE_SOURCES
    src/core/socket.cpp
    src/detail/socket_flags.cpp
    src/core/ip_address.cpp
    src/core/endpoint.cpp
    src/protocol/tcp/tcp_socket.cpp
)

# Platform-specific sources
set(NETLIB_PLATFORM_SOURCES)

if(WIN32)
  list(APPEND NETLIB_PLATFORM_SOURCES
        src/core/detail/socket_windows.cpp
    )
else()
  list(APPEND NETLIB_PLATFORM_SOURCES
        src/core/detail/socket_posix.cpp
    )
endif()

# Combine all sources
set(NETLIB_SOURCES
    ${NETLIB_CORE_SOURCES}
    ${NETLIB_PLATFORM_SOURCES}
)

# ============================================================
# Library
# ============================================================

add_library(NetLib ${NETLIB_SOURCES})

target_include_directories(NetLib
    PUBLIC
        include
)

# Platform-specific linking
if(WIN32)
  target_compile_definitions(NetLib PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(NetLib PRIVATE ws2_32)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(NetLib PRIVATE Threads::Threads)
endif()

# ============================================================
# Testing (Catch2)
# ============================================================

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
)

FetchContent_MakeAvailable(Catch2)

set(NETLIB_TEST_SOURCES
    tests/socket_test.cpp
    tests/tcp_socket_test.cpp
    tests/ip_address_test.cpp
    tests/endpoint_test.cpp
)

add_executable(NetLib_test ${NETLIB_TEST_SOURCES})

target_link_libraries(NetLib_test
    PRIVATE
        NetLib
        Catch2::Catch2WithMain
)

if(WIN32)
  target_compile_definitions(NetLib_test PRIVATE CATCH_CONFIG_NO_WINDOWS_H  WIN32_LEAN_AND_MEAN
            NOMINMAX)
endif()

# ============================================================
# CTest Integration
# ============================================================

enable_testing()
include(CTest)
include(Catch)

catch_discover_tests(NetLib_test)

