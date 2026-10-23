add_executable(xie_desktop_surface_test
    Source/DesktopSurfaceTest.cpp
)

target_compile_features(xie_desktop_surface_test PRIVATE cxx_std_20)

target_include_directories(xie_desktop_surface_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Engine
)

target_link_libraries(xie_desktop_surface_test
    PRIVATE
        xie_engine
)

if (WIN32)
    add_custom_command(TARGET xie_desktop_surface_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:xie_engine>
            $<TARGET_FILE_DIR:xie_desktop_surface_test>
        COMMENT "Copy engine runtime for desktop surface test"
        VERBATIM
    )
endif()

if (BUILD_TESTING)
    add_test(NAME xie_desktop_surface_test COMMAND xie_desktop_surface_test --self-test)
endif()
