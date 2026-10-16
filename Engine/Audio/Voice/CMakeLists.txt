add_library(xie_audio_voice STATIC
    VoiceProcessor.cpp
)
target_include_directories(xie_audio_voice PUBLIC ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_audio_voice
    PUBLIC xie_audio_dsp xie_audio_diagnostics xie_audio_analysis
)
target_compile_features(xie_audio_voice PUBLIC cxx_std_20)
