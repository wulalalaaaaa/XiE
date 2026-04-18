cmake_minimum_required(VERSION 3.20)

# Must be set before project() so MSVC runtime selection is honored globally.
if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(XiE VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC: unify runtime library to avoid LNK4098 (MSVCRT conflicts)
# Debug  -> /MDd
# Release-> /MD
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# spdlog is mandatory for this project
if (NOT EXISTS "${CMAKE_SOURCE_DIR}/ThirdLibraries/spdlog/CMakeLists.txt")
    message(FATAL_ERROR "Missing required submodule: ThirdLibraries/spdlog. Run: git submodule update --init --recursive")
endif()

add_subdirectory(ThirdLibraries/spdlog)
add_subdirectory(Engine)
add_subdirectory(Runtime)

# Visual Studio: default startup target should be runnable app, not ALL_BUILD
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT xie_runtime)
