add_library(xie_engine
    Core/Application.cpp
    Core/Application.h
    Core/Log.cpp
    Core/Log.h
    Platform/Window.cpp
    Platform/Window.h
    Renderer/Renderer.cpp
    Renderer/Renderer.h
)

target_compile_features(xie_engine PUBLIC cxx_std_20)

target_include_directories(xie_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

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

target_link_libraries(xie_engine
    PUBLIC
        glfw
)
