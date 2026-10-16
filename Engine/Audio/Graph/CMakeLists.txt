add_library(xie_audio_graph STATIC
    AudioGraph.cpp
)
target_include_directories(xie_audio_graph PUBLIC ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_audio_graph PUBLIC xie_audio_core)
target_compile_features(xie_audio_graph PUBLIC cxx_std_20)

