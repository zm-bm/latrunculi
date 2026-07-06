cmake_minimum_required(VERSION 3.23)
project(Latrunculi VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" OFF)
include(CTest)
find_package(Threads REQUIRED)

option(LATRUNCULI_SEARCH_STATS "Enable detailed search statistics diagnostics" OFF)

set(LATRUNCULI_ENGINE_SOURCES
    src/board.cpp
    src/board_fen.cpp
    src/board_representation.cpp
    src/board_see.cpp
    src/engine.cpp
    src/evaluator.cpp
    src/fen.cpp
    src/magic.cpp
    src/move.cpp
    src/move_picker.cpp
    src/movegen.cpp
    src/perft.cpp
    src/root_line.cpp
    src/search.cpp
    src/search_worker.cpp
    src/search_limits.cpp
    src/threading.cpp
    src/tt.cpp
    src/uci_input.cpp
    src/uci_options.cpp
    src/uci_writer.cpp
    src/zobrist.cpp
)

add_library(latrunculi_lib OBJECT ${LATRUNCULI_ENGINE_SOURCES})
target_include_directories(latrunculi_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(latrunculi_lib PUBLIC LATRUNCULI_SEARCH_STATS=$<BOOL:${LATRUNCULI_SEARCH_STATS}>)
target_compile_definitions(latrunculi_lib PRIVATE LATRUNCULI_VERSION="${PROJECT_VERSION}")

add_executable(latrunculi src/main.cpp)
target_link_libraries(latrunculi PRIVATE latrunculi_lib Threads::Threads)

option(LATRUNCULI_BUILD_BENCHMARK "Build benchmark target" OFF)
if(LATRUNCULI_BUILD_BENCHMARK)
    add_executable(benchmark bench/benchmark.cpp)
    target_link_libraries(benchmark PRIVATE latrunculi_lib Threads::Threads)
    target_compile_definitions(benchmark PRIVATE LATRUNCULI_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
endif()

if(BUILD_TESTING)
    add_subdirectory(third_party/googletest)

    set(LATRUNCULI_TEST_SOURCES
        tests/bb.test.cpp
        tests/board.test.cpp
        tests/board_fen.test.cpp
        tests/board_representation.test.cpp
        tests/board_see.test.cpp
        tests/engine.test.cpp
        tests/eval.test.cpp
        tests/evaluator.test.cpp
        tests/fen.test.cpp
        tests/magic.test.cpp
        tests/main.cpp
        tests/move.test.cpp
        tests/move_list.test.cpp
        tests/move_ordering.test.cpp
        tests/move_picker.test.cpp
        tests/movegen.test.cpp
        tests/perft.test.cpp
        tests/principal_variation.test.cpp
        tests/score.test.cpp
        tests/search.test.cpp
        tests/search_limits.test.cpp
        tests/search_instrumentation.test.cpp
        tests/search_worker.test.cpp
        tests/threading.test.cpp
        tests/tt.test.cpp
        tests/types.test.cpp
        tests/uci_input.test.cpp
        tests/uci_options.test.cpp
        tests/uci_writer.test.cpp
    )

    add_executable(tests ${LATRUNCULI_TEST_SOURCES})
    target_link_libraries(tests PRIVATE latrunculi_lib gtest gmock Threads::Threads)
    add_test(NAME unit_tests COMMAND tests)
endif()

set(LATRUNCULI_RUNTIME_DIR ${CMAKE_BINARY_DIR}/bin)
set_target_properties(latrunculi PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LATRUNCULI_RUNTIME_DIR})
if(TARGET benchmark)
    set_target_properties(benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LATRUNCULI_RUNTIME_DIR})
endif()
if(TARGET tests)
    set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${LATRUNCULI_RUNTIME_DIR})
endif()
