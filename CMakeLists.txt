cmake_minimum_required(VERSION 3.10)
project(Latrunculi)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(include)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create the executable
add_executable(Latrunculi ${SOURCES})

# Enable testing
# enable_testing()
# add_subdirectory(tests)