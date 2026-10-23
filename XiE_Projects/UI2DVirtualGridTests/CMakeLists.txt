add_executable(xie_ui2d_virtual_grid_tests
    Source/UI2DVirtualGridTests.cpp)
target_compile_features(xie_ui2d_virtual_grid_tests PRIVATE cxx_std_20)
target_include_directories(xie_ui2d_virtual_grid_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_ui2d_virtual_grid_tests PRIVATE xie_ui2d)
if (BUILD_TESTING)
    add_test(NAME xie_ui2d_virtual_grid_tests
        COMMAND xie_ui2d_virtual_grid_tests)
endif()

add_executable(xie_ui2d_virtual_grid_test
    Source/UI2DVirtualGridVisualTest.cpp)
target_compile_features(xie_ui2d_virtual_grid_test PRIVATE cxx_std_20)
target_include_directories(xie_ui2d_virtual_grid_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_ui2d_virtual_grid_test PRIVATE xie_ui2d)
