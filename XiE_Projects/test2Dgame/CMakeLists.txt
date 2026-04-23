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

