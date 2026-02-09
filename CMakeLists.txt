cmake_minimum_required(VERSION 3.20)
project(
  NetLib
  VERSION 0.1
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Source files ----
set(NETLIB_SOURCES src/detail/socket.cpp src/detail/socket_flags.cpp src/detail/ip_address.cpp src/endpoint.cpp src/tcp_socket.cpp)

# Platform-specific source
if(WIN32)
  list(APPEND NETLIB_SOURCES src/detail/socket_windows.cpp)
else()
  list(APPEND NETLIB_SOURCES src/detail/socket_posix.cpp)
endif()

# ---- Define the library ----
add_library(NetLib ${NETLIB_SOURCES})

# ---- Public headers ----
target_include_directories(NetLib PUBLIC include)

# ---- Platform-specific linking ----
if(WIN32)
  add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(NetLib PRIVATE ws2_32)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(NetLib PRIVATE Threads::Threads)
endif()

# ---- Catch2 for testing ----
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0)
FetchContent_MakeAvailable(Catch2)

# ---- Minimal Test ----
add_executable(NetLib_test tests/socket_test.cpp tests/tcp_socket_test.cpp)
target_link_libraries(NetLib_test PRIVATE NetLib Catch2::Catch2WithMain)
if(WIN32)
  target_compile_definitions(NetLib_test PRIVATE CATCH_CONFIG_NO_WINDOWS_H)
endif()

# ---- Enable testing ----
enable_testing()
include(CTest)
include(Catch)
catch_discover_tests(NetLib_test)
