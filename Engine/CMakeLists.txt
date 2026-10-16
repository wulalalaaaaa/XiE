# 自动收集 Engine 源文件，新增文件后重新配置即可出现在 VS
file(GLOB_RECURSE XIE_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]ThirdLibraries[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]External[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Runtime[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Launch[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Network[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Foundation[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Application[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Input[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Renderer2D[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Runtime2D[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]UI2D[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Platform[\\\\/]GLFW[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Platform[\\\\/]Win32[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Audio[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Curve[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Launch[\\\\/].*")
list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Image[\\\\/].*")

set(XIE_ENGINE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(xie_foundation INTERFACE)
target_include_directories(xie_foundation INTERFACE ${XIE_ENGINE_INCLUDE_DIR})
target_compile_features(xie_foundation INTERFACE cxx_std_20)

add_library(xie_platform_contracts INTERFACE)
target_include_directories(xie_platform_contracts INTERFACE ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_platform_contracts INTERFACE xie_foundation)

# Platform-neutral image payload, codec and clipboard service contracts.  The
# concrete Windows implementations are separate optional targets below.
add_library(xie_image STATIC
    Image/ImageBuffer.cpp
    Image/ImageCodec.cpp
    Image/ImageScale.cpp
    Image/ImageFile.cpp
    Image/ImageAsync.cpp
    Image/ImageServices.cpp
)
target_include_directories(xie_image PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_image PUBLIC xie_foundation)
target_compile_features(xie_image PUBLIC cxx_std_20)

add_library(xie_input STATIC
    Input/InputDiagnostics.cpp
    Input/InputEventQueue.cpp
)
target_include_directories(xie_input PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_input PUBLIC xie_foundation xie_platform_contracts)
target_compile_features(xie_input PUBLIC cxx_std_20)

# Generic canvas and transport modules are kept separate from the legacy
# xie_engine aggregate so projects can consume them without pulling in a
# renderer or application business code.
add_library(xie_infinite_canvas STATIC
    UI2D/InfiniteCanvas.cpp
    UI2D/SelectionModel.cpp
    UI2D/ToolController.cpp
    UI2D/InkStroke.cpp
)
target_include_directories(xie_infinite_canvas PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_infinite_canvas PUBLIC xie_foundation xie_input)
target_compile_features(xie_infinite_canvas PUBLIC cxx_std_20)

add_library(xie_network STATIC
    Network/Json.cpp
    Network/HttpClient.cpp
    Network/WebSocketClient.cpp
)
target_include_directories(xie_network PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_network PUBLIC xie_foundation)
target_compile_features(xie_network PUBLIC cxx_std_20)
if (WIN32)
    target_link_libraries(xie_network PUBLIC winhttp)
endif()

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

# Optional composition boundary between platform-neutral ImageBuffer data and
# renderer-owned compact RGBA8 textures.  This target deliberately has no WIC,
# COM, clipboard or platform-backend dependency.
add_library(xie_image_renderer2d STATIC
    Image/Renderer2D/ImageTextureBridge.cpp
)
target_include_directories(xie_image_renderer2d PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_image_renderer2d PUBLIC
    xie_image xie_renderer2d_resources)
target_compile_features(xie_image_renderer2d PUBLIC cxx_std_20)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Runtime2D/World2D.cpp")
    add_library(xie_runtime2d STATIC
        Runtime2D/World2D.cpp
        Runtime2D/MovementSystem2D.cpp
        Runtime2D/CollisionSystem2D.cpp
    )
    target_include_directories(xie_runtime2d PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
    target_link_libraries(xie_runtime2d PUBLIC xie_foundation)
    target_compile_features(xie_runtime2d PUBLIC cxx_std_20)
else()
    # The current source snapshot does not contain Runtime2D. Keep the target
    # name for downstream compatibility without fabricating business sources.
    add_library(xie_runtime2d INTERFACE)
    target_link_libraries(xie_runtime2d INTERFACE xie_foundation)
endif()

add_library(xie_renderer2d_opengl STATIC
    Core/Log.cpp
    Renderer/OpenGLBackend.cpp
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
list(FILTER XIE_UI2D_SOURCES EXCLUDE REGEX ".*[\\/]UI2D[\\/]Audio[\\/].*")
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

# Optional desktop integration contracts.  This target contains only the
# portable service facade; shell/IPC libraries are introduced by the explicit
# platform backend below and are never a consequence of ApplicationContext.
add_library(xie_desktop STATIC
    Application/DesktopServices.cpp
)
target_include_directories(xie_desktop PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_desktop PUBLIC xie_foundation xie_input)
target_compile_features(xie_desktop PUBLIC cxx_std_20)

# World2DDrawBuilder remains a compatibility/project helper because it also depends
# on the legacy Camera2D module. It is intentionally not part of UI2D.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Runtime2D/World2D.h")
    list(APPEND XIE_ENGINE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Renderer2D/World2DDrawBuilder.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Renderer2D/World2DDrawBuilder.h"
    )
else()
    list(FILTER XIE_ENGINE_SOURCES EXCLUDE REGEX ".*[\\\\/]Renderer2D[\\\\/]World2DDrawBuilder\\.(cpp|h)$")
endif()

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
target_link_libraries(xie_renderer2d_opengl PUBLIC glad glfw PRIVATE spdlog::spdlog)

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
        Platform/Windows/TextInput/WindowsTextClipboard.cpp
        Platform/Windows/TextInput/WindowsTextInputMethod.cpp
    )
    target_include_directories(xie_platform_win32 PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
    target_link_libraries(xie_platform_win32 PUBLIC xie_platform_contracts PRIVATE xie_input user32 imm32)
    target_compile_features(xie_platform_win32 PUBLIC cxx_std_20)

    add_library(xie_desktop_win32 STATIC
        Application/Windows/Win32DesktopBackend.cpp
    )
    target_include_directories(xie_desktop_win32 PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
    target_link_libraries(xie_desktop_win32 PUBLIC
        xie_desktop xie_foundation user32 shell32 advapi32)
    target_compile_features(xie_desktop_win32 PUBLIC cxx_std_20)

    add_library(xie_image_wic STATIC
        Image/Windows/WicImageCodec.cpp
    )
    target_include_directories(xie_image_wic PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
    target_link_libraries(xie_image_wic PUBLIC xie_image PRIVATE
        windowscodecs ole32 uuid)
    target_compile_features(xie_image_wic PUBLIC cxx_std_20)

    add_library(xie_image_clipboard_win32 STATIC
        Image/Windows/Win32ImageClipboardSource.cpp
    )
    target_include_directories(xie_image_clipboard_win32 PUBLIC
        ${XIE_ENGINE_INCLUDE_DIR})
    target_link_libraries(xie_image_clipboard_win32 PUBLIC xie_image PRIVATE
        user32)
    target_compile_features(xie_image_clipboard_win32 PUBLIC cxx_std_20)

    add_library(xie_image_win32 STATIC
        Image/Windows/ImageServicesWin32.cpp
    )
    target_include_directories(xie_image_win32 PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
    target_link_libraries(xie_image_win32 PUBLIC
        xie_image_wic xie_image_clipboard_win32)
    target_compile_features(xie_image_win32 PUBLIC cxx_std_20)
endif()

# Task-oriented application composition lives above the existing application
# loop and below project/business code.  Keeping it in a separate target
# avoids making xie_application depend on concrete platform backends.
add_library(xie_application_composition STATIC
    Application/ApplicationContext.cpp
)
target_include_directories(xie_application_composition PUBLIC ${XIE_ENGINE_INCLUDE_DIR})
target_link_libraries(xie_application_composition PUBLIC
    xie_application
    xie_image
    xie_desktop
    xie_input
    xie_renderer2d_contracts
    xie_renderer2d_resources
    xie_renderer2d_opengl
    xie_renderer2d_software
    xie_ui2d
    xie_platform_glfw
)
if (WIN32)
    target_link_libraries(xie_application_composition PUBLIC xie_platform_win32)
endif()
target_compile_features(xie_application_composition PUBLIC cxx_std_20)

if (BUILD_TESTING)
    add_executable(xie_image_contract_tests
        Image/ImageBufferContractTests.cpp)
    target_compile_features(xie_image_contract_tests PRIVATE cxx_std_20)
    target_link_libraries(xie_image_contract_tests PRIVATE xie_image)
    add_test(NAME xie_image_contract_tests COMMAND xie_image_contract_tests)

    add_executable(xie_image_codec_contract_tests
        Image/ImageCodecContractTests.cpp)
    target_compile_features(xie_image_codec_contract_tests PRIVATE cxx_std_20)
    target_link_libraries(xie_image_codec_contract_tests PRIVATE xie_image)
    add_test(NAME xie_image_codec_contract_tests
        COMMAND xie_image_codec_contract_tests)

    add_executable(xie_image_file_contract_tests
        Image/ImageFileContractTests.cpp)
    target_compile_features(xie_image_file_contract_tests PRIVATE cxx_std_20)
    target_link_libraries(xie_image_file_contract_tests PRIVATE xie_image)
    add_test(NAME xie_image_file_contract_tests
        COMMAND xie_image_file_contract_tests)

    add_executable(xie_image_async_contract_tests
        Image/ImageAsyncContractTests.cpp)
    target_compile_features(xie_image_async_contract_tests PRIVATE cxx_std_20)
    target_link_libraries(xie_image_async_contract_tests PRIVATE xie_image)
    add_test(NAME xie_image_async_contract_tests
        COMMAND xie_image_async_contract_tests)

    add_executable(xie_image_renderer2d_contract_tests
        Image/Renderer2D/ImageTextureBridgeContractTests.cpp)
    target_compile_features(xie_image_renderer2d_contract_tests PRIVATE cxx_std_20)
    target_link_libraries(xie_image_renderer2d_contract_tests PRIVATE
        xie_image_renderer2d)
    add_test(NAME xie_image_renderer2d_contract_tests
        COMMAND xie_image_renderer2d_contract_tests)

    add_executable(xie_desktop_services_contract_tests
        Application/DesktopServicesContractTests.cpp)
    target_compile_features(xie_desktop_services_contract_tests PRIVATE cxx_std_20)
    target_link_libraries(xie_desktop_services_contract_tests PRIVATE xie_desktop)
    add_test(NAME xie_desktop_services_contract_tests
        COMMAND xie_desktop_services_contract_tests)

    add_executable(xie_desktop_services_fault_injection_tests
        Application/DesktopServicesFaultInjectionTests.cpp)
    target_compile_features(xie_desktop_services_fault_injection_tests PRIVATE cxx_std_20)
    target_link_libraries(xie_desktop_services_fault_injection_tests PRIVATE
        xie_desktop)
    add_test(NAME xie_desktop_services_fault_injection_tests
        COMMAND xie_desktop_services_fault_injection_tests)

    add_executable(xie_application_context_contract_tests
        Application/ApplicationContextContractTests.cpp)
    target_compile_features(xie_application_context_contract_tests PRIVATE cxx_std_20)
    target_link_libraries(xie_application_context_contract_tests PRIVATE
        xie_application_composition)
    add_test(NAME xie_application_context_contract_tests
        COMMAND xie_application_context_contract_tests)

    add_executable(xie_application_transparent_sample
        Application/XiETransparentWindowSample.cpp)
    target_compile_features(xie_application_transparent_sample PRIVATE cxx_std_20)
    target_link_libraries(xie_application_transparent_sample PRIVATE
        xie_application_composition)
    add_test(NAME xie_application_transparent_sample
        COMMAND xie_application_transparent_sample)

    add_executable(xie_dual_window_workflow_tests
        Application/XiEDualWindowWorkflowTests.cpp)
    target_compile_features(xie_dual_window_workflow_tests PRIVATE cxx_std_20)
    target_link_libraries(xie_dual_window_workflow_tests PRIVATE
        xie_application_composition)
    add_test(NAME xie_dual_window_workflow_tests
        COMMAND xie_dual_window_workflow_tests)

    add_executable(xie_interactive_overlay_contract_tests
        Application/InteractiveOverlayContractTests.cpp)
    target_compile_features(xie_interactive_overlay_contract_tests PRIVATE
        cxx_std_20)
    target_link_libraries(xie_interactive_overlay_contract_tests PRIVATE
        xie_application_composition)
    add_test(NAME xie_interactive_overlay_contract_tests
        COMMAND xie_interactive_overlay_contract_tests)

    if (WIN32)
        add_executable(xie_image_wic_tests
            Image/Windows/WicImageCodecTests.cpp)
        target_compile_features(xie_image_wic_tests PRIVATE cxx_std_20)
        target_link_libraries(xie_image_wic_tests PRIVATE xie_image_wic)
        add_test(NAME xie_image_wic_tests COMMAND xie_image_wic_tests)

        add_executable(xie_image_file_wic_tests
            Image/Windows/ImageFileWicTests.cpp)
        target_compile_features(xie_image_file_wic_tests PRIVATE cxx_std_20)
        target_link_libraries(xie_image_file_wic_tests PRIVATE
            xie_image_wic)
        add_test(NAME xie_image_file_wic_tests
            COMMAND xie_image_file_wic_tests)

        add_executable(xie_image_renderer2d_wic_tests
            Image/Windows/ImageTextureBridgeWicTests.cpp)
        target_compile_features(xie_image_renderer2d_wic_tests PRIVATE cxx_std_20)
        target_link_libraries(xie_image_renderer2d_wic_tests PRIVATE
            xie_image_renderer2d xie_image_wic)
        add_test(NAME xie_image_renderer2d_wic_tests
            COMMAND xie_image_renderer2d_wic_tests)

        add_executable(xie_image_clipboard_contract_tests
            Image/Windows/Win32ImageClipboardContractTests.cpp)
        target_compile_features(xie_image_clipboard_contract_tests PRIVATE
            cxx_std_20)
        target_link_libraries(xie_image_clipboard_contract_tests PRIVATE
            xie_image_clipboard_win32)
        add_test(NAME xie_image_clipboard_contract_tests
            COMMAND xie_image_clipboard_contract_tests)

        add_executable(xie_desktop_instance_helper
            Application/Windows/DesktopInstanceHelper.cpp)
        target_compile_features(xie_desktop_instance_helper PRIVATE cxx_std_20)
        target_link_libraries(xie_desktop_instance_helper PRIVATE
            xie_application_composition xie_desktop_win32)

        add_executable(xie_desktop_instance_dual_process_tests
            Application/Windows/DesktopInstanceDualProcessTests.cpp)
        target_compile_features(xie_desktop_instance_dual_process_tests PRIVATE cxx_std_20)
        add_test(NAME xie_desktop_instance_dual_process_tests
            COMMAND xie_desktop_instance_dual_process_tests
                $<TARGET_FILE:xie_desktop_instance_helper>)

        # Opt-in native smoke: it briefly creates a real tray icon, hotkey,
        # and single-instance endpoint.  It is buildable by default but only
        # performs the desktop mutation when XIE_RUN_DESKTOP_SMOKE=1 is set.
        add_executable(xie_desktop_native_smoke
            Application/Windows/DesktopServicesNativeSmoke.cpp)
        target_compile_features(xie_desktop_native_smoke PRIVATE cxx_std_20)
        target_link_libraries(xie_desktop_native_smoke PRIVATE
            xie_application_composition xie_desktop_win32)

        # Opt-in native message smoke for the public Interactive Overlay
        # composition.  Product code remains platform-neutral; only this
        # test tool inspects the real HWND and posts test messages.
        add_executable(xie_interactive_overlay_native_smoke
            Application/Windows/InteractiveOverlayNativeSmoke.cpp)
        target_compile_features(xie_interactive_overlay_native_smoke PRIVATE
            cxx_std_20)
        target_link_libraries(xie_interactive_overlay_native_smoke PRIVATE
            xie_application_composition)
    endif()
endif()

if (WIN32)
    # Opt-in, read-only native clipboard smoke.  It never writes or clears the
    # user's clipboard and is intentionally not registered with CTest.
    add_executable(xie_image_native_clipboard_smoke
        Image/Windows/ImageClipboardNativeSmoke.cpp)
    target_compile_features(xie_image_native_clipboard_smoke PRIVATE cxx_std_20)
    target_link_libraries(xie_image_native_clipboard_smoke PRIVATE
        xie_image_win32)
endif()

target_link_libraries(xie_engine
    PUBLIC
        xie_application
        xie_image
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
    target_link_libraries(xie_engine PUBLIC xie_platform_win32 opengl32 windowscodecs ole32 uuid)
    target_link_libraries(xie_renderer2d_opengl PUBLIC opengl32)
    target_link_libraries(xie_renderer2d_software PUBLIC user32 gdi32)
    target_link_libraries(xie_renderer2d_resources PUBLIC user32 gdi32)
    target_link_libraries(xie_platform_glfw PRIVATE imm32)
endif()

add_subdirectory(Curve)
add_subdirectory(Audio)
add_subdirectory(UI2D/Audio)
