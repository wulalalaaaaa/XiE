add_library(xie_curve STATIC
    Curve.cpp
)

target_include_directories(xie_curve PUBLIC ${CMAKE_SOURCE_DIR}/Engine)
target_compile_features(xie_curve PUBLIC cxx_std_20)

