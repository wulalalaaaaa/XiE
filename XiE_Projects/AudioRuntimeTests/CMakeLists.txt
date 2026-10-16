add_executable(xie_audio_runtime_tests
    Source/AudioRuntimeTests.cpp
)
target_compile_features(xie_audio_runtime_tests PRIVATE cxx_std_20)
target_include_directories(xie_audio_runtime_tests PRIVATE ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_audio_runtime_tests PRIVATE
    xie_curve
    xie_audio_core
    xie_audio_graph
    xie_audio_dsp
    xie_audio_diagnostics
    xie_audio_voice
)

if (BUILD_TESTING)
    add_test(NAME xie_audio_runtime_tests COMMAND xie_audio_runtime_tests)
endif()

add_executable(xie_parametric_eq_spectrum_tests
    Source/ParametricEQAndSpectrumTests.cpp
)
target_compile_features(xie_parametric_eq_spectrum_tests PRIVATE cxx_std_20)
target_include_directories(xie_parametric_eq_spectrum_tests PRIVATE ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_parametric_eq_spectrum_tests PRIVATE
    xie_audio_dsp
    xie_audio_analysis
)
if (BUILD_TESTING)
    add_test(NAME xie_parametric_eq_spectrum_tests COMMAND xie_parametric_eq_spectrum_tests)
endif()

add_executable(xie_audio_rt_safety_tests
    Source/RealtimeSafetyTests.cpp
)
target_compile_features(xie_audio_rt_safety_tests PRIVATE cxx_std_20)
target_include_directories(xie_audio_rt_safety_tests PRIVATE ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_audio_rt_safety_tests PRIVATE
    xie_audio_graph
    xie_audio_dsp
    xie_audio_voice
)
if (BUILD_TESTING)
    add_test(NAME xie_audio_rt_safety_tests COMMAND xie_audio_rt_safety_tests)
endif()

add_executable(xie_noise_suppression_tests
    Source/NoiseSuppressionTests.cpp
)
target_compile_features(xie_noise_suppression_tests PRIVATE cxx_std_20)
target_include_directories(xie_noise_suppression_tests PRIVATE ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_noise_suppression_tests PRIVATE
    xie_audio_dsp
)
if (BUILD_TESTING)
    add_test(NAME xie_noise_suppression_tests COMMAND xie_noise_suppression_tests)
endif()

add_executable(xie_haze_effects_tests
    Source/HazeEffectsTests.cpp
)
target_compile_features(xie_haze_effects_tests PRIVATE cxx_std_20)
target_include_directories(xie_haze_effects_tests PRIVATE ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_haze_effects_tests PRIVATE
    xie_audio_dsp
    xie_audio_voice
)
if (BUILD_TESTING)
    add_test(NAME xie_haze_effects_tests COMMAND xie_haze_effects_tests)
endif()

add_executable(xie_synthetic_voice_tests
    Source/SyntheticVoiceTests.cpp
)
target_compile_features(xie_synthetic_voice_tests PRIVATE cxx_std_20)
target_include_directories(xie_synthetic_voice_tests PRIVATE ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_synthetic_voice_tests PRIVATE
    xie_audio_dsp
    xie_audio_voice
)
if (BUILD_TESTING)
    add_test(NAME xie_synthetic_voice_tests COMMAND xie_synthetic_voice_tests)
endif()

add_executable(xie_electronic_mist_tests
    Source/ElectronicMistTests.cpp
)
target_compile_features(xie_electronic_mist_tests PRIVATE cxx_std_20)
target_include_directories(xie_electronic_mist_tests PRIVATE ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_electronic_mist_tests PRIVATE
    xie_audio_dsp
    xie_audio_voice
)
if (BUILD_TESTING)
    add_test(NAME xie_electronic_mist_tests COMMAND xie_electronic_mist_tests)
endif()

add_executable(XiEAudioBenchmark
    Source/AudioBenchmark.cpp
)
target_compile_features(XiEAudioBenchmark PRIVATE cxx_std_20)
target_include_directories(XiEAudioBenchmark PRIVATE ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(XiEAudioBenchmark PRIVATE
    xie_audio_graph
    xie_audio_dsp
    xie_audio_voice
)

if (WIN32)
    add_executable(XiEAudioSmoke
        Source/AudioSmoke.cpp
    )
    target_compile_features(XiEAudioSmoke PRIVATE cxx_std_20)
    target_include_directories(XiEAudioSmoke PRIVATE ${CMAKE_SOURCE_DIR}/Engine)
    target_link_libraries(XiEAudioSmoke PRIVATE
        xie_audio_backend_wasapi
        xie_audio_voice
        xie_audio_diagnostics
    )
    if (BUILD_TESTING)
        add_test(NAME xie_audio_smoke_self_test COMMAND XiEAudioSmoke --self-test)
    endif()
endif()
