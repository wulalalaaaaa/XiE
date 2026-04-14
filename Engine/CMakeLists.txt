add_library(xie_engine
    Core/Application.cpp
    Core/Log.cpp
    Platform/Window.cpp
    Renderer/Renderer.cpp
)

target_include_directories(xie_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(glfw3 CONFIG REQUIRED)

target_link_libraries(xie_engine
    PUBLIC
        glfw
)
