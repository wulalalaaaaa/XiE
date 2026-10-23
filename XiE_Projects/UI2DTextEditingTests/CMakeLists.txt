add_executable(xie_ui2d_text_editing_tests
    Source/UI2DTextEditingTests.cpp)
target_compile_features(xie_ui2d_text_editing_tests PRIVATE cxx_std_20)
target_link_libraries(xie_ui2d_text_editing_tests PRIVATE
    xie_ui2d xie_renderer2d_resources)
if (BUILD_TESTING)
    add_test(NAME xie_ui2d_text_editing_tests
        COMMAND xie_ui2d_text_editing_tests)
endif()
