cmake_minimum_required(VERSION 3.23)
project(Latrunculi VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" OFF)
include(CTest)
find_package(Threads REQUIRED)

option(LATRUNCULI_SEARCH_STATS "Enable search diagnostics" OFF)
option(LATRUNCULI_TUNING "Expose search parameters for SPSA" OFF)
option(LATRUNCULI_USE_POPCNT "Require hardware POPCNT on x86-64" ON)
include(cmake/SliderBackend.cmake)
# Ship the fixed external network beside the executables. It is read once at startup.
configure_file(networks/nn-82215d0fd0df.nnue nn-82215d0fd0df.nnue COPYONLY)
option(LATRUNCULI_EMBED_NETWORK "Embed the pinned network for single-file deployment" OFF)
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
    src/eval/nnue.cpp
    src/eval/nnue_network.cpp
    src/eval/nnue_avx2.cpp
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
if(LATRUNCULI_EMBED_NETWORK)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "Embedded network builds require Linux with GCC or Clang")
    endif()
    enable_language(ASM)
    set(NNUE_ASM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/networks/nn-82215d0fd0df.nnue")
    configure_file(cmake/NnueBlob.S.in NnueBlob.S @ONLY)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/NnueBlob.S"
        PROPERTIES OBJECT_DEPENDS "${NNUE_ASM_PATH}")
    target_sources(latrunculi_lib PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/NnueBlob.S")
    target_compile_definitions(latrunculi_lib PRIVATE LATRUNCULI_EMBED_NETWORK=1)
endif()
target_compile_definitions(latrunculi_lib PUBLIC LATRUNCULI_TUNING=$<BOOL:${LATRUNCULI_TUNING}>)
if(LATRUNCULI_TUNING)
    target_sources(latrunculi_lib PRIVATE src/tuning/options.cpp)
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
    add_executable(latrunculi-neural-verify tools/nnue/verify.cpp)
    target_link_libraries(latrunculi-neural-verify PRIVATE latrunculi_lib)

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
        tests/eval/nnue.test.cpp
        tests/eval/nnue_network.test.cpp
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

    if(LATRUNCULI_TUNING)
        list(APPEND LATRUNCULI_TEST_SOURCES tests/tuning/options.test.cpp)
    endif()
    add_executable(tests
        src/cli/cli.cpp
        ${LATRUNCULI_TEST_SOURCES}
    )
    target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(tests PRIVATE latrunculi_lib gtest gmock)
    set_property(TARGET tests PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ${LATRUNCULI_RELEASE_IPO})
    add_test(NAME unit_tests COMMAND tests)

    add_executable(latrunculi-stress tests/stress/randomized.test.cpp)
    target_include_directories(latrunculi-stress PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(latrunculi-stress PRIVATE latrunculi_lib gtest)
    set_property(TARGET latrunculi-stress PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ${LATRUNCULI_RELEASE_IPO})
    add_test(NAME randomized_stress COMMAND latrunculi-stress)
endif()
