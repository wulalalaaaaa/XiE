cmake_minimum_required(VERSION 3.20)

set(CRYSTAL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
file(GLOB_RECURSE CRYSTAL_CORE_SOURCES CONFIGURE_DEPENDS
    "${CRYSTAL_SOURCE_DIR}/*.cpp"
    "${CRYSTAL_SOURCE_DIR}/*.h"
)
list(FILTER CRYSTAL_CORE_SOURCES EXCLUDE REGEX ".*[\\/]Main\\.cpp$")
list(FILTER CRYSTAL_CORE_SOURCES EXCLUDE REGEX ".*[\\/]CrystalRenderer\\.cpp$")

add_library(crystal_core STATIC ${CRYSTAL_CORE_SOURCES})
target_compile_features(crystal_core PUBLIC cxx_std_20)
target_include_directories(crystal_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_SOURCE_DIR}/Engine"
)
target_link_libraries(crystal_core PUBLIC xie_infinite_canvas xie_network)
if (MSVC)
    target_compile_options(crystal_core PRIVATE /utf-8)
endif()

add_executable(crystal_client
    Source/Main.cpp
    Source/UI/CrystalRenderer.cpp
)
target_compile_features(crystal_client PRIVATE cxx_std_20)
target_include_directories(crystal_client PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_SOURCE_DIR}/Engine"
)
target_link_libraries(crystal_client PRIVATE
    crystal_core
    xie_application
    xie_platform_glfw
    xie_input
    glfw
    glad
)
if (WIN32)
    target_link_libraries(crystal_client PRIVATE opengl32 user32)
endif()
if (MSVC)
    target_compile_options(crystal_client PRIVATE /utf-8)
endif()
set_target_properties(crystal_client PROPERTIES FOLDER "XiE_Projects/Crystal")

if (BUILD_TESTING)
    add_executable(crystal_core_tests Tests/CrystalCoreTests.cpp)
    target_compile_features(crystal_core_tests PRIVATE cxx_std_20)
    target_include_directories(crystal_core_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
        "${CMAKE_SOURCE_DIR}/Engine"
    )
    target_link_libraries(crystal_core_tests PRIVATE crystal_core xie_application)
    add_test(NAME crystal_core_tests COMMAND crystal_core_tests)
    set_target_properties(crystal_core_tests PROPERTIES FOLDER "XiE_Projects/Crystal")
endif()
