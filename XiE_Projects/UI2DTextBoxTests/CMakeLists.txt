add_executable(xie_ui2d_text_box_tests
    Source/UI2DTextBoxTests.cpp)
target_compile_features(xie_ui2d_text_box_tests PRIVATE cxx_std_20)
target_link_libraries(xie_ui2d_text_box_tests PRIVATE
    xie_ui2d xie_renderer2d_resources)
if (BUILD_TESTING)
    add_test(NAME xie_ui2d_text_box_tests
        COMMAND xie_ui2d_text_box_tests)
endif()

add_executable(xie_ui2d_text_box_test
    Source/UI2DTextBoxVisualTest.cpp)
target_compile_features(xie_ui2d_text_box_test PRIVATE cxx_std_20)
target_link_libraries(xie_ui2d_text_box_test PRIVATE
    xie_ui2d xie_renderer2d_resources)

add_executable(xie_ui2d_text_box_performance
    Source/UI2DTextBoxPerformance.cpp)
target_compile_features(xie_ui2d_text_box_performance PRIVATE cxx_std_20)
target_link_libraries(xie_ui2d_text_box_performance PRIVATE
    xie_ui2d xie_renderer2d_resources)
