cmake_minimum_required(VERSION 3.10)
project(Latrunculi)

option(DEBUG_STATS "Enable debug search statistics" OFF)
if(DEBUG_STATS)
    add_compile_definitions(DEBUG_STATS)
endif()

# Specify C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

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

# Enable testing
enable_testing()

# Add Google Test submodule
add_subdirectory(third_party/googletest)

# Test files
file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")

# Create test executable
add_executable(tests ${TEST_SOURCES})

# Link GTest to the test executable
target_link_libraries(tests lib gtest gmock gtest_main pthread)

# Add tests
include(CTest)
add_test(NAME tests COMMAND tests)