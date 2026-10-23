cmake_minimum_required(VERSION 3.20)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(XiESensoryInterface VERSION 1.0.0 LANGUAGES CXX)
endif()

include(CTest)
find_package(Threads REQUIRED)

add_library(xie_sensory_interface STATIC
    Source/SensoryEvent.cpp
    Source/SensoryEventChannel.cpp
    Source/StreamSequenceValidator.cpp
    Source/SimulatedAdapters.cpp
)
target_compile_features(xie_sensory_interface PUBLIC cxx_std_20)
target_include_directories(xie_sensory_interface
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include
)
target_link_libraries(xie_sensory_interface
    PUBLIC
        Threads::Threads
)

add_executable(xie_sensory_demo Demo/Main.cpp)
target_compile_features(xie_sensory_demo PRIVATE cxx_std_20)
target_link_libraries(xie_sensory_demo PRIVATE xie_sensory_interface)

add_executable(xie_sensory_benchmarks Benchmarks/LatestOnlyBenchmark.cpp)
target_compile_features(xie_sensory_benchmarks PRIVATE cxx_std_20)
target_link_libraries(xie_sensory_benchmarks PRIVATE xie_sensory_interface)

if (BUILD_TESTING)
    add_executable(xie_sensory_tests Tests/SensoryInterfaceTests.cpp)
    target_compile_features(xie_sensory_tests PRIVATE cxx_std_20)
    target_link_libraries(xie_sensory_tests PRIVATE xie_sensory_interface)
    add_test(NAME xie_sensory_tests COMMAND xie_sensory_tests)
    set_tests_properties(xie_sensory_tests PROPERTIES TIMEOUT 60)
endif()

install(TARGETS xie_sensory_interface
    ARCHIVE DESTINATION lib
    COMPONENT XSI
)
install(TARGETS xie_sensory_demo
    RUNTIME DESTINATION .
    COMPONENT XSI
)
install(DIRECTORY Include/ DESTINATION include COMPONENT XSI)
