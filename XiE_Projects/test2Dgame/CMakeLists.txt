file(GLOB_RECURSE XIE_TEST2DGAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h"
)

add_library(xie_game_test2d STATIC
    ${XIE_TEST2DGAME_SOURCES}
)

target_compile_features(xie_game_test2d PUBLIC cxx_std_20)

target_include_directories(xie_game_test2d
    PUBLIC
        ${CMAKE_SOURCE_DIR}/Engine
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(xie_game_test2d
    PUBLIC
        xie_engine
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${XIE_TEST2DGAME_SOURCES})

add_executable(xie_story_tests
    Tests/StoryTests.cpp
)

target_compile_features(xie_story_tests PRIVATE cxx_std_20)

target_include_directories(xie_story_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Engine
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(xie_story_tests
    PRIVATE
        xie_game_test2d
)

set_target_properties(xie_story_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

if (WIN32)
    add_custom_command(TARGET xie_story_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:xie_engine>
            $<TARGET_FILE_DIR:xie_story_tests>
        COMMENT "Copy engine runtime for story tests"
        VERBATIM
    )
endif()

if (BUILD_TESTING)
    add_test(NAME xie_story_tests COMMAND xie_story_tests)
    set_tests_properties(xie_story_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
