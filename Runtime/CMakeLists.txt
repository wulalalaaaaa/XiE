add_executable(xie_runtime
    main.cpp
)

target_compile_features(xie_runtime PRIVATE cxx_std_17)

target_link_libraries(xie_runtime
    PRIVATE
        xie_engine
)
