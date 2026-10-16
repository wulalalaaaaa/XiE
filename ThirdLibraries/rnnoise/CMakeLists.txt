# XiE build wrapper for the unmodified RNNoise 0.2 release sources.
add_library(xie_thirdparty_rnnoise STATIC
    src/denoise.c
    src/rnn.c
    src/pitch.c
    src/kiss_fft.c
    src/celt_lpc.c
    src/nnet.c
    src/nnet_default.c
    src/parse_lpcnet_weights.c
    src/rnnoise_data.c
    src/rnnoise_tables.c
)

target_include_directories(xie_thirdparty_rnnoise
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_definitions(xie_thirdparty_rnnoise PRIVATE
    RNNOISE_BUILD=1
    DISABLE_DEBUG_FLOAT=1
)
set_target_properties(xie_thirdparty_rnnoise PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    FOLDER "ThirdLibraries"
)

# The first XiE integration deliberately uses RNNoise's portable scalar path.
# This avoids making AVX2/SSE4.1 a runtime requirement on older Windows CPUs.
if (MSVC)
    target_compile_definitions(xie_thirdparty_rnnoise PRIVATE
        _CRT_SECURE_NO_WARNINGS
        restrict=__restrict
    )
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        # RNNoise's own x86_arch_macros.h maps this to __SSE2__ for MSVC.
        # SSE2 is part of the Windows x64 ABI; AVX/AVX2 remain disabled.
        target_compile_definitions(xie_thirdparty_rnnoise PRIVATE
            OPUS_X86_MAY_HAVE_SSE2=1
        )
    endif()
    target_compile_options(xie_thirdparty_rnnoise PRIVATE /W0)
endif()
