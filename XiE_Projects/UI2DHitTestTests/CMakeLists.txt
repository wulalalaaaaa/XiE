add_executable(xie_ui2d_hit_test_tests
    Source/UI2DHitTestTests.cpp
)

target_compile_features(xie_ui2d_hit_test_tests PRIVATE cxx_std_20)
target_include_directories(xie_ui2d_hit_test_tests PRIVATE ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_ui2d_hit_test_tests PRIVATE xie_ui2d)

if (BUILD_TESTING)
    add_test(NAME xie_ui2d_hit_test_tests COMMAND xie_ui2d_hit_test_tests)
endif()
