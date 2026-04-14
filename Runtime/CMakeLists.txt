add_executable(xie_runtime
    main.cpp
)

target_link_libraries(xie_runtime
    PRIVATE
        xie_engine
)
