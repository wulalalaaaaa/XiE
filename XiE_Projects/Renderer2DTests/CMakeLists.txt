add_executable(xie_renderer2d_tests
    Source/Renderer2DTests.cpp
)

target_compile_features(xie_renderer2d_tests PRIVATE cxx_std_20)

target_include_directories(xie_renderer2d_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Engine
)

target_link_libraries(xie_renderer2d_tests
    PRIVATE
        xie_engine
)

if (WIN32)
    add_custom_command(TARGET xie_renderer2d_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:xie_engine>
            $<TARGET_FILE_DIR:xie_renderer2d_tests>
        COMMENT "Copy engine runtime for renderer2d tests"
        VERBATIM
    )
endif()

if (BUILD_TESTING)
    add_test(NAME xie_renderer2d_tests COMMAND xie_renderer2d_tests)
endif()
