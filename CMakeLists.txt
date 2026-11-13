cmake_minimum_required(VERSION 3.23)
project(Latrunculi VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" OFF)
include(CTest)
find_package(Threads REQUIRED)

option(LATRUNCULI_SEARCH_STATS "Enable search diagnostics" OFF)
option(LATRUNCULI_USE_POPCNT "Require hardware POPCNT on x86-64" ON)
include(cmake/SliderBackend.cmake)
option(LATRUNCULI_NNUE "Use the experimental embedded NNUE evaluator" OFF)
set(LATRUNCULI_NNUE_NETWORK "" CACHE FILEPATH "Frozen NNUE network to embed")
set(LATRUNCULI_NNUE_NETWORK_SHA256 "" CACHE STRING "SHA256 of an already validated frozen NNUE network")
if(LATRUNCULI_NNUE)
    if(NOT EXISTS "${LATRUNCULI_NNUE_NETWORK}" OR IS_DIRECTORY "${LATRUNCULI_NNUE_NETWORK}")
        message(FATAL_ERROR "NNUE requires an explicit existing LATRUNCULI_NNUE_NETWORK")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${LATRUNCULI_NNUE_NETWORK}" "${CMAKE_SOURCE_DIR}/tools/nnue/format.py")
    if(LATRUNCULI_NNUE_NETWORK_SHA256)
        # Workers need no Python when building an already validated, pinned artifact.
        # The native decoder still checks its format and arithmetic bounds on load.
        file(SHA256 "${LATRUNCULI_NNUE_NETWORK}" nnue_sha256)
        if(NOT nnue_sha256 STREQUAL LATRUNCULI_NNUE_NETWORK_SHA256)
            message(FATAL_ERROR "Frozen NNUE network SHA256 mismatch")
        endif()
        file(READ "${LATRUNCULI_NNUE_NETWORK}" nnue_hex HEX)
        string(LENGTH "${nnue_hex}" nnue_hex_size)
        math(EXPR nnue_size "${nnue_hex_size} / 2")
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," nnue_bytes "${nnue_hex}")
        file(WRITE "${CMAKE_BINARY_DIR}/nnue_embedded.hpp"
            "#pragma once\n#include <array>\n#include <cstdint>\nnamespace nnue::embedded {\n"
            "inline constexpr std::array<std::uint8_t, ${nnue_size}> bytes = {${nnue_bytes}};\n"
            "inline constexpr char sha256[] = \"${nnue_sha256}\";\n}\n")
    else()
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/nnue/format.py"
            "${LATRUNCULI_NNUE_NETWORK}" --embed "${CMAKE_BINARY_DIR}/nnue_embedded.hpp"
            RESULT_VARIABLE nnue_export_result ERROR_VARIABLE nnue_export_error)
        if(NOT nnue_export_result EQUAL 0)
            message(FATAL_ERROR "Invalid NNUE network: ${nnue_export_error}")
        endif()
    endif()
endif()
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

# Enable Release IPO by default for our targets. Honor CMake's standard
# overrides, e.g. -DCMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE=OFF.
# Keep the default local so GoogleTest does not inherit it.
set(LATRUNCULI_RELEASE_IPO ON)
if(DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE)
    set(LATRUNCULI_RELEASE_IPO "${CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE}")
elseif(DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    set(LATRUNCULI_RELEASE_IPO "${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")
endif()

set(LATRUNCULI_ENGINE_SOURCES
    src/core/attacks_sliding.cpp
    src/core/move.cpp
    src/core/notation.cpp
    src/board/board_fen.cpp
    src/board/board_move.cpp
    src/board/board_representation.cpp
    src/board/board_rules.cpp
    src/board/board_see.cpp
    src/board/fen_parser.cpp
    src/board/notation.cpp
    src/movegen/move_sources.cpp
    src/movegen/perft.cpp
    src/eval/evaluation.cpp
    src/eval/features.cpp
    src/search/root_search.cpp
    src/search/search.cpp
    src/search/instrumentation.cpp
    src/search/thread_pool.cpp
    src/search/time_manager.cpp
    src/search/tt.cpp
    src/search/worker.cpp
    src/uci/engine.cpp
    src/uci/options.cpp
    src/uci/parser.cpp
    src/uci/writer.cpp
)

add_library(latrunculi_lib OBJECT ${LATRUNCULI_ENGINE_SOURCES})
if(LATRUNCULI_NNUE)
    target_sources(latrunculi_lib PRIVATE src/eval/nnue.cpp)
    target_include_directories(latrunculi_lib PRIVATE "${CMAKE_BINARY_DIR}")
    target_compile_definitions(latrunculi_lib PUBLIC LATRUNCULI_NNUE=1)
    add_executable(latrunculi-neural-verify tools/nnue/verify.cpp)
    target_link_libraries(latrunculi-neural-verify PRIVATE latrunculi_lib)
endif()
set_property(TARGET latrunculi_lib PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ${LATRUNCULI_RELEASE_IPO})
target_include_directories(latrunculi_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(latrunculi_lib PUBLIC LATRUNCULI_SEARCH_STATS=$<BOOL:${LATRUNCULI_SEARCH_STATS}>)
target_compile_definitions(latrunculi_lib PUBLIC LATRUNCULI_VERSION="${PROJECT_VERSION}")
target_link_libraries(latrunculi_lib PUBLIC Threads::Threads)

target_compile_definitions(latrunculi_lib PUBLIC LATRUNCULI_USE_PEXT=$<BOOL:${LATRUNCULI_USE_PEXT}>)
if(LATRUNCULI_USE_PEXT)
    target_compile_options(latrunculi_lib PUBLIC -mbmi2)
endif()

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
    src/cli/cli.cpp
)
target_link_libraries(latrunculi PRIVATE latrunculi_lib)
set_property(TARGET latrunculi PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ${LATRUNCULI_RELEASE_IPO})

option(LATRUNCULI_BUILD_BENCHMARK "Build native search benchmark" OFF)
if(LATRUNCULI_BUILD_BENCHMARK)
    add_executable(latrunculi-benchmark tools/benchmark.cpp)
    target_compile_definitions(latrunculi-benchmark PRIVATE
        LATRUNCULI_SEARCH_SUITE="${CMAKE_CURRENT_SOURCE_DIR}/tools/resources/search.epd"
    )
    target_link_libraries(latrunculi-benchmark PRIVATE latrunculi_lib)
    set_property(TARGET latrunculi-benchmark PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ${LATRUNCULI_RELEASE_IPO})
endif()

if(BUILD_TESTING)
    add_subdirectory(third_party/googletest)

    set(LATRUNCULI_TEST_SOURCES
        tests/main.cpp
        tests/core/attacks.test.cpp
        tests/core/attacks_sliding.test.cpp
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
        tests/cli/cli.test.cpp
        tests/movegen/generator.test.cpp
        tests/movegen/moves.test.cpp
        tests/movegen/perft.test.cpp
        tests/eval/evaluation.test.cpp
        tests/eval/features.test.cpp
        tests/eval/parameters.test.cpp
        tests/eval/tapered_score.test.cpp
        tests/search/search.test.cpp
        tests/search/history.test.cpp
        tests/search/instrumentation.test.cpp
        tests/search/limits.test.cpp
        tests/search/pv.test.cpp
        tests/search/quiescence.test.cpp
        tests/search/root_search.test.cpp
        tests/search/thread_pool.test.cpp
        tests/search/time_manager.test.cpp
        tests/search/tt.test.cpp
        tests/search/tt_clock.test.cpp
        tests/search/worker.test.cpp
        tests/uci/engine.test.cpp
        tests/uci/engine_options.test.cpp
        tests/uci/engine_position.test.cpp
        tests/uci/engine_search.test.cpp
        tests/uci/options.test.cpp
        tests/uci/parser.test.cpp
        tests/uci/writer.test.cpp
    )

    if(LATRUNCULI_NNUE)
        # HCE score/reconstruction fixtures belong to the full default build.
        # NNUE has an explicit export refusal and its own evaluator contracts.
        list(REMOVE_ITEM LATRUNCULI_TEST_SOURCES
            tests/eval/evaluation.test.cpp tests/eval/features.test.cpp tests/cli/cli.test.cpp)
        list(APPEND LATRUNCULI_TEST_SOURCES tests/eval/nnue.test.cpp)
    endif()

    add_executable(tests
        src/cli/cli.cpp
        ${LATRUNCULI_TEST_SOURCES}
    )
    target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    if(LATRUNCULI_NNUE)
        target_include_directories(tests PRIVATE ${CMAKE_BINARY_DIR})
    endif()
    target_link_libraries(tests PRIVATE latrunculi_lib gtest gmock)
    set_property(TARGET tests PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ${LATRUNCULI_RELEASE_IPO})
    add_test(NAME unit_tests COMMAND tests)

    add_executable(latrunculi-stress tests/stress/randomized.test.cpp)
    target_include_directories(latrunculi-stress PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(latrunculi-stress PRIVATE latrunculi_lib gtest)
    set_property(TARGET latrunculi-stress PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ${LATRUNCULI_RELEASE_IPO})
    add_test(NAME randomized_stress COMMAND latrunculi-stress)
endif()
