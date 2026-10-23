add_executable(xie_runtime_host_tests
    Source/RuntimeHostTests.cpp
)

target_compile_features(xie_runtime_host_tests PRIVATE cxx_std_20)

target_include_directories(xie_runtime_host_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Engine
)

target_link_libraries(xie_runtime_host_tests
    PRIVATE
        xie_engine
)

if (WIN32)
    add_custom_command(TARGET xie_runtime_host_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:xie_engine>
            $<TARGET_FILE_DIR:xie_runtime_host_tests>
        COMMENT "Copy engine runtime for host tests"
        VERBATIM
    )
endif()

if (BUILD_TESTING)
    add_test(NAME xie_runtime_host_tests COMMAND xie_runtime_host_tests)
endif()
