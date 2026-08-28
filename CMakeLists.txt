cmake_minimum_required(VERSION 3.23)
project(Latrunculi VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" OFF)
include(CTest)
find_package(Threads REQUIRED)

option(LATRUNCULI_SEARCH_STATS "Enable detailed search statistics diagnostics" OFF)
option(LATRUNCULI_USE_POPCNT "Require hardware POPCNT on x86-64" ON)
set(LATRUNCULI_SANITIZER "none" CACHE STRING "Sanitizer configuration")
set_property(CACHE LATRUNCULI_SANITIZER PROPERTY STRINGS none address-undefined thread)

set(LATRUNCULI_VALID_SANITIZERS none address-undefined thread)
if(NOT LATRUNCULI_SANITIZER IN_LIST LATRUNCULI_VALID_SANITIZERS)
    message(FATAL_ERROR
        "Unsupported LATRUNCULI_SANITIZER=${LATRUNCULI_SANITIZER}; "
        "expected none, address-undefined, or thread")
endif()

if(NOT LATRUNCULI_SANITIZER STREQUAL "none"
   AND NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR
        "LATRUNCULI_SANITIZER requires a GNU or Clang-compatible compiler")
endif()

set(LATRUNCULI_ENGINE_SOURCES
    src/core/attacks_magic.cpp
    src/core/move.cpp
    src/core/notation.cpp
    src/board/board_fen.cpp
    src/board/board_move.cpp
    src/board/board_representation.cpp
    src/board/board_rules.cpp
    src/board/board_see.cpp
    src/board/fen_parser.cpp
    src/board/notation.cpp
    src/movegen/perft.cpp
    src/eval/evaluation.cpp
    src/eval/features.cpp
    src/search/algorithm.cpp
    src/search/instrumentation.cpp
    src/search/limits.cpp
    src/search/ordering/picker.cpp
    src/search/root_line.cpp
    src/search/thread_pool.cpp
    src/search/tt.cpp
    src/search/worker.cpp
    src/uci/engine.cpp
    src/uci/options.cpp
    src/uci/parser.cpp
    src/uci/writer.cpp
)

add_library(latrunculi_lib OBJECT ${LATRUNCULI_ENGINE_SOURCES})
target_include_directories(latrunculi_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(latrunculi_lib PUBLIC LATRUNCULI_SEARCH_STATS=$<BOOL:${LATRUNCULI_SEARCH_STATS}>)
target_compile_definitions(latrunculi_lib PUBLIC LATRUNCULI_VERSION="${PROJECT_VERSION}")
target_link_libraries(latrunculi_lib PUBLIC Threads::Threads)

if(LATRUNCULI_SANITIZER STREQUAL "address-undefined")
    target_compile_options(latrunculi_lib PUBLIC
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(latrunculi_lib INTERFACE -fsanitize=address,undefined)
elseif(LATRUNCULI_SANITIZER STREQUAL "thread")
    target_compile_options(latrunculi_lib PUBLIC
        -fsanitize=thread
        -fno-omit-frame-pointer
    )
    target_link_options(latrunculi_lib INTERFACE -fsanitize=thread)
endif()

if(LATRUNCULI_USE_POPCNT
   AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$"
   AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(latrunculi_lib PUBLIC -mpopcnt)
endif()

add_executable(latrunculi
    src/main.cpp
    src/bench/benchmark.cpp
)
target_link_libraries(latrunculi PRIVATE latrunculi_lib)

option(LATRUNCULI_BUILD_MEASUREMENTS "Build component measurement tool" OFF)
if(LATRUNCULI_BUILD_MEASUREMENTS)
    add_executable(latrunculi-measure
        tools/measurements/main.cpp
        tools/measurements/eval.cpp
        tools/measurements/perft.cpp
        tools/measurements/search.cpp
    )
    target_link_libraries(latrunculi-measure PRIVATE latrunculi_lib)
endif()

if(BUILD_TESTING)
    add_subdirectory(third_party/googletest)

    set(LATRUNCULI_TEST_SOURCES
        tests/main.cpp
        tests/core/attacks.test.cpp
        tests/core/attacks_magic.test.cpp
        tests/core/bitboard.test.cpp
        tests/core/move.test.cpp
        tests/core/move_geometry.test.cpp
        tests/core/notation.test.cpp
        tests/core/piece.test.cpp
        tests/core/square.test.cpp
        tests/core/types.test.cpp
        tests/board/board_fen.test.cpp
        tests/board/board_move.test.cpp
        tests/board/board_representation.test.cpp
        tests/board/board_rules.test.cpp
        tests/board/board_see.test.cpp
        tests/board/fen_parser.test.cpp
        tests/board/notation.test.cpp
        tests/movegen/generator.test.cpp
        tests/movegen/move_list.test.cpp
        tests/movegen/perft.test.cpp
        tests/eval/evaluation.test.cpp
        tests/eval/features.test.cpp
        tests/eval/parameters.test.cpp
        tests/eval/tapered_score.test.cpp
        tests/search/algorithm.test.cpp
        tests/search/instrumentation.test.cpp
        tests/search/limits.test.cpp
        tests/search/ordering/history.test.cpp
        tests/search/ordering/picker.test.cpp
        tests/search/ordering/refutations.test.cpp
        tests/search/ordering/state.test.cpp
        tests/search/principal_variation.test.cpp
        tests/search/quiescence.test.cpp
        tests/search/root_line.test.cpp
        tests/search/root_search.test.cpp
        tests/search/thread_pool.test.cpp
        tests/search/tt.test.cpp
        tests/search/worker.test.cpp
        tests/uci/engine.test.cpp
        tests/uci/engine_options.test.cpp
        tests/uci/engine_position.test.cpp
        tests/uci/engine_search.test.cpp
        tests/uci/options.test.cpp
        tests/uci/parser.test.cpp
        tests/uci/writer.test.cpp
    )

    add_executable(tests ${LATRUNCULI_TEST_SOURCES})
    target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(tests PRIVATE latrunculi_lib gtest gmock)
    add_test(NAME unit_tests COMMAND tests)

    add_executable(latrunculi-stress tests/stress/randomized.test.cpp)
    target_include_directories(latrunculi-stress PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(latrunculi-stress PRIVATE latrunculi_lib gtest)
    add_test(NAME randomized_stress COMMAND latrunculi-stress)
endif()
