cmake_minimum_required(VERSION 3.10)
project(Latrunculi)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(include)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")

# Create object library from source files
add_library(LatrunculiLib OBJECT ${SOURCES})

# Create the main executable
add_executable(Latrunculi src/main.cpp)
target_link_libraries(Latrunculi LatrunculiLib)

# Enable testing
enable_testing()

# Add Google Test submodule
add_subdirectory(third_party/googletest)

# Include Google Test directories
include_directories(third_party/googletest/googletest/include)

# Test files
file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")

# Create test executable
add_executable(runTests ${TEST_SOURCES})

# Link GTest to the test executable
target_link_libraries(runTests LatrunculiLib gtest gtest_main pthread)

# Add tests
add_test(NAME LatrunculiTests COMMAND runTests)
