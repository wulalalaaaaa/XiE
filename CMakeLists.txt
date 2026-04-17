cmake_minimum_required(VERSION 3.20)
project(XiE VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# spdlog is mandatory for this project
if (NOT EXISTS "${CMAKE_SOURCE_DIR}/ThirdLibraries/spdlog/CMakeLists.txt")
    message(FATAL_ERROR "Missing required submodule: ThirdLibraries/spdlog. Run: git submodule update --init --recursive")
endif()

add_subdirectory(ThirdLibraries/spdlog)

add_subdirectory(Engine)
add_subdirectory(Runtime)
