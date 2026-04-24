cmake_minimum_required(VERSION 3.23)
project(Latrunculi VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(Threads REQUIRED)

set(LATRUNCULI_ENGINE_SOURCES
    src/board.cpp
    src/engine.cpp
    src/evaluator.cpp
    src/fen.cpp
    src/magic.cpp
    src/move.cpp
    src/movegen.cpp
    src/search.cpp
    src/search_options.cpp
    src/thread.cpp
    src/thread_pool.cpp
    src/tt.cpp
    src/uci.cpp
    src/zobrist.cpp
)

add_library(latrunculi_lib OBJECT ${LATRUNCULI_ENGINE_SOURCES})
target_include_directories(latrunculi_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(latrunculi_lib PRIVATE LATRUNCULI_VERSION="${PROJECT_VERSION}")

add_executable(latrunculi src/main.cpp)
target_link_libraries(latrunculi PRIVATE latrunculi_lib Threads::Threads)

option(LATRUNCULI_BUILD_BENCHMARK "Build benchmark target" ON)
if(LATRUNCULI_BUILD_BENCHMARK)
    add_executable(benchmark bench/benchmark.cpp)
    target_link_libraries(benchmark PRIVATE latrunculi_lib Threads::Threads)
endif()

if(BUILD_TESTING)
    add_subdirectory(third_party/googletest)

    set(LATRUNCULI_TEST_SOURCES
        tests/bb.test.cpp
        tests/board.test.cpp
        tests/engine.test.cpp
        tests/eval.test.cpp
        tests/evaluator.test.cpp
        tests/fen.test.cpp
        tests/history.test.cpp
        tests/killers.test.cpp
        tests/magic.test.cpp
        tests/main.cpp
        tests/move.test.cpp
        tests/move_list.test.cpp
        tests/movegen.test.cpp
        tests/score.test.cpp
        tests/search.test.cpp
        tests/search_options.test.cpp
        tests/search_stats.test.cpp
        tests/thread.test.cpp
        tests/thread_pool.test.cpp
        tests/tt.test.cpp
        tests/types.test.cpp
        tests/uci.test.cpp
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
