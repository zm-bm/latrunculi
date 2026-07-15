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
    src/core/attacks_magic.cpp
    src/core/notation.cpp
    src/board/board.cpp
    src/board/board_fen.cpp
    src/board/board_representation.cpp
    src/board/board_see.cpp
    src/board/fen.cpp
    src/board/zobrist.cpp
    src/core/move.cpp
    src/eval/evaluator.cpp
    src/movegen/movegen.cpp
    src/movegen/perft.cpp
    src/search/move_picker.cpp
    src/search/root_line.cpp
    src/search/search.cpp
    src/search/search_limits.cpp
    src/search/search_worker.cpp
    src/search/tt.cpp
    src/uci/engine.cpp
    src/uci/threading.cpp
    src/uci/uci_input.cpp
    src/uci/uci_options.cpp
    src/uci/uci_writer.cpp
)

add_library(latrunculi_lib OBJECT ${LATRUNCULI_ENGINE_SOURCES})
target_include_directories(latrunculi_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(latrunculi_lib PUBLIC LATRUNCULI_SEARCH_STATS=$<BOOL:${LATRUNCULI_SEARCH_STATS}>)
target_compile_definitions(latrunculi_lib PUBLIC LATRUNCULI_VERSION="${PROJECT_VERSION}")

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
        tests/core/attacks.test.cpp
        tests/core/attacks_magic.test.cpp
        tests/board/board.test.cpp
        tests/board/board_fen.test.cpp
        tests/board/board_representation.test.cpp
        tests/board/board_see.test.cpp
        tests/board/fen.test.cpp
        tests/core/bitboard.test.cpp
        tests/core/move.test.cpp
        tests/core/notation.test.cpp
        tests/core/piece.test.cpp
        tests/core/square.test.cpp
        tests/core/types.test.cpp
        tests/eval/eval.test.cpp
        tests/eval/evaluator.test.cpp
        tests/eval/tapered_score.test.cpp
        tests/main.cpp
        tests/movegen/move_list.test.cpp
        tests/movegen/movegen.test.cpp
        tests/movegen/perft.test.cpp
        tests/search/history.test.cpp
        tests/search/move_ordering.test.cpp
        tests/search/move_picker.test.cpp
        tests/search/principal_variation.test.cpp
        tests/search/search.test.cpp
        tests/search/search_instrumentation.test.cpp
        tests/search/search_limits.test.cpp
        tests/search/search_worker.test.cpp
        tests/search/tt.test.cpp
        tests/uci/engine.test.cpp
        tests/uci/threading.test.cpp
        tests/uci/uci_input.test.cpp
        tests/uci/uci_options.test.cpp
        tests/uci/uci_writer.test.cpp
    )

    add_executable(tests ${LATRUNCULI_TEST_SOURCES})
    target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
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
