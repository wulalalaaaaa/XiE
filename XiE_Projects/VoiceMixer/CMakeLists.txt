set(VOICEMIXER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(EMOEXEC_CONFIGURATION_DIR
    "${CMAKE_SOURCE_DIR}/XiE_Projects/EmoExec/EmoExec/Source/Infrastructure/Configuration")

# Reuse the existing XiE-project JSON parser and atomic replacement writer.
# This target is static and adds no EmoExec runtime/application dependency.
add_library(xie_settings_json STATIC
    "${EMOEXEC_CONFIGURATION_DIR}/JsonValue.cpp"
    "${EMOEXEC_CONFIGURATION_DIR}/AtomicFileWriter.cpp"
)
target_compile_features(xie_settings_json PUBLIC cxx_std_20)
target_include_directories(xie_settings_json PUBLIC
    "${CMAKE_SOURCE_DIR}/XiE_Projects/EmoExec/EmoExec/Source")

add_library(xie_voicemixer_app STATIC
    Source/App/VoiceMixerAudioController.cpp
    Source/Preset/VoicePreset.cpp
    Source/UI/UICurveEditor.cpp
    Source/UI/VoiceMixerWindow.cpp
)
target_compile_features(xie_voicemixer_app PUBLIC cxx_std_20)
if (MSVC)
    target_compile_options(xie_voicemixer_app PRIVATE /utf-8)
endif()
target_include_directories(xie_voicemixer_app PUBLIC
    "${VOICEMIXER_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/Engine"
)
target_link_libraries(xie_voicemixer_app PUBLIC
    xie_settings_json
    xie_curve
    xie_audio_core
    xie_audio_backend_wasapi
    xie_audio_diagnostics
    xie_audio_voice
    xie_input
    xie_renderer2d_contracts
    xie_renderer2d_resources
    xie_renderer2d_software
    xie_ui2d_audio
    xie_platform_win32
)

add_executable(XiEVoiceMixer WIN32 Source/Main.cpp)
target_compile_features(XiEVoiceMixer PRIVATE cxx_std_20)
if (MSVC)
    target_compile_options(XiEVoiceMixer PRIVATE /utf-8)
endif()
target_link_libraries(XiEVoiceMixer PRIVATE xie_voicemixer_app user32 shell32 comctl32)

set_target_properties(XiEVoiceMixer PROPERTIES
    OUTPUT_NAME "XiEVoiceMixer"
    FOLDER "XiE_Projects/VoiceMixer"
)

if (BUILD_TESTING)
    add_executable(xie_voicemixer_tests Tests/VoiceMixerTests.cpp)
    target_compile_features(xie_voicemixer_tests PRIVATE cxx_std_20)
    target_link_libraries(xie_voicemixer_tests PRIVATE xie_voicemixer_app)
    target_compile_definitions(xie_voicemixer_tests PRIVATE
        VOICEMIXER_TEST_ROOT="${CMAKE_CURRENT_BINARY_DIR}/TestData")
    add_test(NAME xie_voicemixer_tests COMMAND xie_voicemixer_tests)
    add_test(NAME xie_voicemixer_ui_smoke COMMAND XiEVoiceMixer --self-test)
endif()

install(TARGETS XiEVoiceMixer RUNTIME DESTINATION .)
install(FILES
    "${CMAKE_SOURCE_DIR}/ThirdPartyNotices.txt"
    "${CMAKE_SOURCE_DIR}/ThirdLibraries/rnnoise/SOURCE.md"
    DESTINATION .
)
install(FILES "${CMAKE_SOURCE_DIR}/ThirdLibraries/rnnoise/COPYING"
    DESTINATION . RENAME "RNNoise-LICENSE.txt")
if (MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
    include(InstallRequiredSystemLibraries)
endif()
