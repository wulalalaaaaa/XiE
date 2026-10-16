add_library(xie_audio_analysis STATIC
    SpectrumAnalyzer.cpp
)
target_include_directories(xie_audio_analysis PUBLIC ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_audio_analysis PUBLIC xie_audio_core)
target_compile_features(xie_audio_analysis PUBLIC cxx_std_20)
