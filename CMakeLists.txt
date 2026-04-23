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

    # Extra hardening for targets that may ignore MSVC_RUNTIME_LIBRARY (some generated C targets)
    add_compile_options(
        $<$<CONFIG:Debug>:/MDd>
        $<$<CONFIG:Release>:/MD>
        $<$<CONFIG:RelWithDebInfo>:/MD>
        $<$<CONFIG:MinSizeRel>:/MD>
    )
endif()

# spdlog is mandatory for this project.
# Prefer the new location under Engine/ThirdLibraries, while keeping a fallback for old layout.
set(XIE_SPDLOG_DIR "${CMAKE_SOURCE_DIR}/Engine/ThirdLibraries/spdlog")
if (NOT EXISTS "${XIE_SPDLOG_DIR}/CMakeLists.txt")
    set(XIE_SPDLOG_DIR "${CMAKE_SOURCE_DIR}/ThirdLibraries/spdlog")
endif()
if (NOT EXISTS "${XIE_SPDLOG_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Missing required submodule: Engine/ThirdLibraries/spdlog (or ThirdLibraries/spdlog). Run: git submodule update --init --recursive")
endif()

add_subdirectory("${XIE_SPDLOG_DIR}" "${CMAKE_BINARY_DIR}/_deps/spdlog")
add_subdirectory(Engine)
add_subdirectory(XiE_Projects/test2Dgame)
add_subdirectory(Engine/Launch)

# Visual Studio: default startup target should be runnable app, not ALL_BUILD
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT xie_launch)
