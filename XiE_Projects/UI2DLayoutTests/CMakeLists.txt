add_executable(xie_ui2d_layout_tests
    Source/UI2DLayoutTests.cpp
)

target_compile_features(xie_ui2d_layout_tests PRIVATE cxx_std_20)
target_include_directories(xie_ui2d_layout_tests PRIVATE ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_ui2d_layout_tests PRIVATE xie_ui2d xie_renderer2d_resources)

if (BUILD_TESTING)
    add_test(NAME xie_ui2d_layout_tests COMMAND xie_ui2d_layout_tests)
endif()
