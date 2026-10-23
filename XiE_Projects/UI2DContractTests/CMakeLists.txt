add_executable(xie_ui2d_contract_tests
    Source/UI2DContractTests.cpp
)

target_compile_features(xie_ui2d_contract_tests PRIVATE cxx_std_20)
target_compile_definitions(xie_ui2d_contract_tests PRIVATE XIE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(xie_ui2d_contract_tests PRIVATE xie_ui2d xie_application)

if (BUILD_TESTING)
    add_test(NAME xie_ui2d_contract_tests COMMAND xie_ui2d_contract_tests)
endif()
