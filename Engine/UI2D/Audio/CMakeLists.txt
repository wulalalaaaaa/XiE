add_library(xie_ui2d_audio STATIC
    UIAudioCurveEditor.cpp
    UIParametricEQEditor.cpp
)
target_include_directories(xie_ui2d_audio PUBLIC ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_ui2d_audio PUBLIC
    xie_audio_dsp
    xie_audio_analysis
    xie_input
    xie_renderer2d_contracts
)
target_compile_features(xie_ui2d_audio PUBLIC cxx_std_20)
