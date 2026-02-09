cmake_minimum_required(VERSION 3.20)
project(MatterThreads LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Project options
option(ENABLE_HW_BRIDGE "Enable hardware bridge mode (requires libcurl)" OFF)
option(ENABLE_DARWIN_BRIDGE "Enable Darwin Framework bridge (macOS/iOS, requires Matter.framework)" OFF)
option(ENABLE_GATEWAY "Enable fleet gateway service (requires ENABLE_HW_BRIDGE)" OFF)

if(ENABLE_GATEWAY AND NOT ENABLE_HW_BRIDGE)
    message(FATAL_ERROR "ENABLE_GATEWAY requires ENABLE_HW_BRIDGE=ON")
endif()
option(ENABLE_TESTS "Build unit and scenario tests" ON)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

# Compiler warnings
include(cmake/CompilerWarnings.cmake)

# AddressSanitizer
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# Third-party dependencies
add_subdirectory(third_party)

# Source libraries and executables
add_subdirectory(src)
add_subdirectory(app)

# Tests
if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
