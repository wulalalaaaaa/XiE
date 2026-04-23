# 自动收集 Engine 源文件，新增文件后重新配置即可出现在 VS
file(GLOB_RECURSE XIE_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]ThirdLibraries[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Runtime[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Launch[\\\\/].*")

# 改为 DLL（共享库）
add_library(xie_engine SHARED
    ${XIE_ENGINE_SOURCES}
)

target_compile_features(xie_engine PUBLIC cxx_std_20)

# Build mode macro for compile-time feature branching in engine code
# Debug-like configs => XIE_DEBUG=1, Release => XIE_DEBUG=0
# (works for multi-config generators like Visual Studio)
target_compile_definitions(xie_engine
    PUBLIC
        $<$<CONFIG:Debug>:XIE_DEBUG=1>
        $<$<CONFIG:RelWithDebInfo>:XIE_DEBUG=1>
        $<$<CONFIG:MinSizeRel>:XIE_DEBUG=0>
        $<$<CONFIG:Release>:XIE_DEBUG=0>
)

target_include_directories(xie_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Windows 下自动导出符号，避免手写 __declspec(dllexport)
set_target_properties(xie_engine PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

# 在 Visual Studio 中按真实目录树显示
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${XIE_ENGINE_SOURCES})

include(FetchContent)

option(XIE_USE_SYSTEM_GLFW "Use system-installed GLFW when available" ON)

if (XIE_USE_SYSTEM_GLFW)
    find_package(glfw3 CONFIG QUIET)
endif()

if (NOT TARGET glfw)
    message(STATUS "GLFW not found on system, fetching GLFW from GitHub...")

    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )

    FetchContent_MakeAvailable(glfw)
endif()

# GLAD: modern OpenGL function loader
# NOTE: glad's helper function `glad_add_library` is defined in its cmake module.
# Make the module available before calling it.
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
)

FetchContent_MakeAvailable(glad)
if (NOT COMMAND glad_add_library)
    add_subdirectory(${glad_SOURCE_DIR}/cmake ${glad_BINARY_DIR})
endif()

glad_add_library(glad REPRODUCIBLE API gl:core=3.3)

target_link_libraries(xie_engine
    PUBLIC
        glfw
        glad
        spdlog::spdlog
)

# Enforce consistent MSVC runtime on all local/third-party targets to avoid LNK4098.
if (MSVC)
    set(_XIE_MSVC_RT "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    set_property(TARGET xie_engine PROPERTY MSVC_RUNTIME_LIBRARY "${_XIE_MSVC_RT}")

    if (TARGET glad)
        set_property(TARGET glad PROPERTY MSVC_RUNTIME_LIBRARY "${_XIE_MSVC_RT}")
    endif()

    if (TARGET glfw)
        set_property(TARGET glfw PROPERTY MSVC_RUNTIME_LIBRARY "${_XIE_MSVC_RT}")
    endif()

    if (TARGET spdlog)
        set_property(TARGET spdlog PROPERTY MSVC_RUNTIME_LIBRARY "${_XIE_MSVC_RT}")
    endif()
endif()

if (WIN32)
    target_link_libraries(xie_engine PUBLIC opengl32)
endif()
