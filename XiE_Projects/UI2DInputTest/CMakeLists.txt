add_executable(xie_ui2d_input_test Source/UI2DInputTest.cpp)
target_compile_features(xie_ui2d_input_test PRIVATE cxx_std_20)
target_include_directories(xie_ui2d_input_test PRIVATE ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_ui2d_input_test PRIVATE
    xie_ui2d
    xie_renderer2d_resources
    xie_renderer2d_opengl
    xie_renderer2d_software
    xie_platform_glfw
)

if(WIN32)
    target_link_libraries(xie_ui2d_input_test PRIVATE xie_platform_win32)
endif()

if(BUILD_TESTING)
    add_test(NAME xie_ui2d_input_test COMMAND xie_ui2d_input_test --self-test)
endif()
