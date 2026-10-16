add_library(xie_audio_core STATIC
    AudioBuffer.cpp
    AudioSpscRingBuffer.cpp
)

target_include_directories(xie_audio_core PUBLIC ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_audio_core PUBLIC xie_curve)
target_compile_features(xie_audio_core PUBLIC cxx_std_20)
