file(GLOB_RECURSE XIE_LAUNCH_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(xie_launch
    ${XIE_LAUNCH_SOURCES}
)

target_compile_features(xie_launch PRIVATE cxx_std_20)

target_link_libraries(xie_launch
    PRIVATE
        xie_engine
        xie_game_test2d
)

set_target_properties(xie_launch PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:xie_launch>"
)

if (WIN32)
    add_custom_command(TARGET xie_launch POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:xie_engine>
            $<TARGET_FILE_DIR:xie_launch>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/XiE_Projects/test2Dgame/Assets"
            "$<TARGET_FILE_DIR:xie_launch>/Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/XiE_Projects/test2Dgame/Config"
            "$<TARGET_FILE_DIR:xie_launch>/Config"
        COMMENT "Copy launch runtime content"
        VERBATIM
    )
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${XIE_LAUNCH_SOURCES})

