# 自动收集 Engine 源文件，新增文件后重新配置即可出现在 VS
file(GLOB_RECURSE XIE_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
<<<<<<< Updated upstream
=======
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]ThirdLibraries[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]External[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Runtime[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Launch[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Foundation[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Application[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Input[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Renderer2D[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Runtime2D[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]UI2D[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Platform[\\\\/]GLFW[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Platform[\\\\/]Win32[\\\\/].*")

set(XIE_ENGINE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(xie_foundation INTERFACE)
target_include_directories(xie_foundation INTERFACE ${XIE_ENGINE_INCLUDE_DIR})
target_compile_features(xie_foundation INTERFACE cxx_std_20)

add_library(xie_platform_contracts INTERFACE)
target_include_directories(xie_platform_contracts INTERFACE ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_platform_contracts INTERFACE xie_foundation)

add_library(xie_input STATIC
    Input/InputEventQueue.cpp
)
target_include_directories(xie_input PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_input PUBLIC xie_foundation xie_platform_contracts)
target_compile_features(xie_input PUBLIC cxx_std_20)

add_library(xie_renderer2d_contracts STATIC
    Renderer2D/DrawList2D.cpp
    Renderer2D/DrawListValidator.cpp
)
target_include_directories(xie_renderer2d_contracts PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_renderer2d_contracts PUBLIC xie_foundation)
target_compile_features(xie_renderer2d_contracts PUBLIC cxx_std_20)

add_library(xie_renderer2d_resources STATIC
    Renderer2D/TextureRegistry2D.cpp
    Renderer2D/TextLayoutService.cpp
)
target_include_directories(xie_renderer2d_resources PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_renderer2d_resources PUBLIC xie_renderer2d_contracts)
target_compile_features(xie_renderer2d_resources PUBLIC cxx_std_20)

add_library(xie_runtime2d STATIC
    Runtime2D/World2D.cpp
    Runtime2D/MovementSystem2D.cpp
    Runtime2D/CollisionSystem2D.cpp
)
target_include_directories(xie_runtime2d PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_runtime2d PUBLIC xie_foundation)
target_compile_features(xie_runtime2d PUBLIC cxx_std_20)

add_library(xie_renderer2d_opengl STATIC
    Renderer2D/Backends/OpenGL/OpenGL2DRenderer.cpp
    Renderer2D/Backends/OpenGL/OpenGLWindowRenderPipeline.cpp
)
target_include_directories(xie_renderer2d_opengl PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_renderer2d_opengl PUBLIC xie_renderer2d_contracts xie_renderer2d_resources xie_platform_contracts)
target_compile_features(xie_renderer2d_opengl PUBLIC cxx_std_20)

add_library(xie_renderer2d_software STATIC
    Renderer2D/Backends/Software/Software2DRenderer.cpp
    Renderer2D/Backends/Software/LayeredWindowPresenter.cpp
    Renderer2D/Backends/Software/LayeredWindowRenderPipeline.cpp
)
target_include_directories(xie_renderer2d_software PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_renderer2d_software PUBLIC xie_renderer2d_contracts xie_renderer2d_resources xie_platform_contracts)
target_compile_features(xie_renderer2d_software PUBLIC cxx_std_20)

file(GLOB_RECURSE XIE_UI2D_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/UI2D/*.cpp")
add_library(xie_ui2d STATIC ${XIE_UI2D_SOURCES})
target_include_directories(xie_ui2d PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_ui2d PUBLIC xie_foundation xie_input xie_renderer2d_contracts)
target_compile_features(xie_ui2d PUBLIC cxx_std_20)

add_library(xie_application STATIC
    Application/ApplicationRuntime.cpp
    Application/ApplicationCommandQueue.cpp
    Application/FrameScheduler.cpp
    Application/WindowRuntimeRegistry.cpp
)
target_include_directories(xie_application PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_application PUBLIC xie_foundation xie_platform_contracts xie_renderer2d_contracts)
target_compile_features(xie_application PUBLIC cxx_std_20)

# World2DDrawBuilder remains a compatibility/project helper because it also depends
# on the legacy Camera2D module. It is intentionally not part of UI2D.
list(APPEND XIE_ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Renderer2D/World2DDrawBuilder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Renderer2D/World2DDrawBuilder.h"
)
>>>>>>> Stashed changes

# 改为 DLL（共享库）
add_library(xie_engine SHARED
    ${XIE_ENGINE_SOURCES}
)

target_compile_features(xie_engine PUBLIC cxx_std_20)

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

FetchContent_GetProperties(glad)
if (NOT glad_POPULATED)
    FetchContent_Populate(glad)
    add_subdirectory(${glad_SOURCE_DIR}/cmake ${glad_BINARY_DIR})
endif()

glad_add_library(glad REPRODUCIBLE API gl:core=3.3)

add_library(xie_platform_glfw STATIC
    Platform/GLFW/GlfwApplicationHost.cpp
    Platform/GLFW/GlfwInputState.cpp
    Platform/GLFW/GlfwOpenGLSurface.cpp
    Platform/GLFW/GlfwWindowSystem.cpp
)
target_include_directories(xie_platform_glfw PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_platform_glfw PUBLIC xie_platform_contracts PRIVATE xie_input glfw)
target_compile_features(xie_platform_glfw PUBLIC cxx_std_20)

if (WIN32)
    add_library(xie_platform_win32 STATIC
        Platform/Win32/Win32ApplicationHost.cpp
        Platform/Win32/Win32WindowSystem.cpp
    )
    target_include_directories(xie_platform_win32 PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
    target_link_libraries(xie_platform_win32 PUBLIC xie_platform_contracts PRIVATE xie_input user32)
    target_compile_features(xie_platform_win32 PUBLIC cxx_std_20)
endif()

target_link_libraries(xie_engine
    PUBLIC
        xie_application
        xie_input
        xie_renderer2d_contracts
        xie_renderer2d_resources
        xie_renderer2d_opengl
        xie_renderer2d_software
        xie_runtime2d
        xie_ui2d
        xie_platform_glfw
        glfw
        glad
        spdlog::spdlog
)

if (WIN32)
<<<<<<< Updated upstream
    target_link_libraries(xie_engine PUBLIC opengl32)
=======
    target_link_libraries(xie_engine PUBLIC xie_platform_win32 opengl32 windowscodecs ole32 uuid)
    target_link_libraries(xie_renderer2d_resources PUBLIC windowscodecs ole32 uuid)
    target_link_libraries(xie_renderer2d_software PUBLIC user32 gdi32)
>>>>>>> Stashed changes
endif()
