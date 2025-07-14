cmake_minimum_required(VERSION 3.10)
project(Latrunculi)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Include directories
include_directories(include)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")

# Create object library from source files
add_library(lib OBJECT ${SOURCES})

# Create the main executable
add_executable(latrunculi src/main.cpp)
target_link_libraries(latrunculi lib)

# Benchmark
file(GLOB_RECURSE BENCH_TEST_SOURCES "bench/*.cpp")
add_executable(benchmark ${BENCH_TEST_SOURCES})
target_link_libraries(benchmark lib)

# Tests
enable_testing()
add_subdirectory(third_party/googletest)
file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
add_executable(tests ${TEST_SOURCES})

# Link GTest to the test executable
target_link_libraries(tests lib gtest gmock pthread)

# Add tests
include(CTest)
add_test(NAME UnitTests COMMAND tests)

# Copy to bin directory after build
set_target_properties(latrunculi benchmark tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
