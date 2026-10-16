add_library(xie_audio_backend_wasapi STATIC
    WasapiBackend.cpp
)

target_include_directories(xie_audio_backend_wasapi PUBLIC ${CMAKE_SOURCE_DIR}/Engine)
target_link_libraries(xie_audio_backend_wasapi
    PUBLIC xie_audio_core
    PRIVATE ole32 uuid avrt
)
target_compile_features(xie_audio_backend_wasapi PUBLIC cxx_std_20)
target_compile_definitions(xie_audio_backend_wasapi PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

