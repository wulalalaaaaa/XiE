add_library(xie_audio_diagnostics STATIC
    AudioDiagnostics.cpp
)
target_include_directories(xie_audio_diagnostics PUBLIC ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_audio_diagnostics PUBLIC xie_audio_core)
target_compile_features(xie_audio_diagnostics PUBLIC cxx_std_20)

