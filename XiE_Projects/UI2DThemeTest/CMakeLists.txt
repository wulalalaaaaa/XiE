add_executable(xie_ui2d_theme_test Source/UI2DThemeTest.cpp)
target_compile_features(xie_ui2d_theme_test PRIVATE cxx_std_20)
target_include_directories(xie_ui2d_theme_test PRIVATE ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_ui2d_theme_test PRIVATE xie_engine)
if(WIN32)
    add_custom_command(TARGET xie_ui2d_theme_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:xie_engine> $<TARGET_FILE_DIR:xie_ui2d_theme_test>)
endif()
if(BUILD_TESTING)
    add_test(NAME xie_ui2d_theme_test COMMAND xie_ui2d_theme_test --self-test)
endif()
