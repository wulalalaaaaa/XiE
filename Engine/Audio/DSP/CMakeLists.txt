add_library(xie_audio_dsp STATIC
    BasicProcessors.cpp
    Delay/DelayLine.cpp
    Diffusion/AllPassDiffuser.cpp
    Diffusion/DiffusionNetwork.cpp
    ElectronicMist/ElectronicMist.cpp
    EQ/ParametricEQ.cpp
    Filter/DampingFilter.cpp
    Formant.cpp
    Haze/SoftHaze.cpp
    Haze/SpatialHaze.cpp
    Noise/NoiseFrameAdapter.cpp
    Noise/NoiseSuppressorProcessor.cpp
    Noise/RNNoiseSuppressor.cpp
    Pitch.cpp
    Synthetic/MicroPitchProcessor.cpp
    Synthetic/PitchStabilizer.cpp
    Synthetic/SyntheticVoice.cpp
)
target_include_directories(xie_audio_dsp PUBLIC ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_audio_dsp PUBLIC xie_audio_graph xie_thirdparty_rnnoise)
target_compile_features(xie_audio_dsp PUBLIC cxx_std_20)
