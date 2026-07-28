cmake_minimum_required(VERSION 3.20)
project(XiE VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# spdlog is mandatory for this project
if (NOT EXISTS "${CMAKE_SOURCE_DIR}/ThirdLibraries/spdlog/CMakeLists.txt")
    message(FATAL_ERROR "Missing required submodule: ThirdLibraries/spdlog. Run: git submodule update --init --recursive")
endif()

add_subdirectory(ThirdLibraries/spdlog)

add_subdirectory(Engine)
<<<<<<< Updated upstream
add_subdirectory(Runtime)
=======
add_subdirectory(XiE_Projects/test2Dgame)
add_subdirectory(XiE_Projects/RuntimeHostTests)
add_subdirectory(XiE_Projects/DesktopSurfaceTest)
add_subdirectory(XiE_Projects/Renderer2DTests)
add_subdirectory(XiE_Projects/UI2DContractTests)
add_subdirectory(XiE_Projects/UI2DLayoutTests)
add_subdirectory(XiE_Projects/UI2DHitTestTests)
add_subdirectory(XiE_Projects/UI2DRenderBuilderTests)
add_subdirectory(XiE_Projects/UI2DRenderTest)
add_subdirectory(XiE_Projects/UI2DInputRouterTests)
add_subdirectory(XiE_Projects/UI2DInputTest)
add_subdirectory(XiE_Projects/UI2DFocusManagerTests)
add_subdirectory(XiE_Projects/UI2DFocusTest)
add_subdirectory(XiE_Projects/UI2DAnimatorTests)
add_subdirectory(XiE_Projects/UI2DAnimatorTest)
add_subdirectory(XiE_Projects/UI2DThemeTests)
add_subdirectory(XiE_Projects/UI2DThemeTest)
add_subdirectory(XiE_Projects/UI2DWidgetTests)
add_subdirectory(XiE_Projects/UI2DWidgetTest)
add_subdirectory(XiE_Projects/UI2DScrollViewTests)
add_subdirectory(XiE_Projects/UI2DScrollViewTest)
add_subdirectory(XiE_Projects/EmoExec/EmoExec)
add_subdirectory(Engine/Launch)

# Visual Studio: default startup target should be runnable app, not ALL_BUILD
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT xie_launch)
>>>>>>> Stashed changes
