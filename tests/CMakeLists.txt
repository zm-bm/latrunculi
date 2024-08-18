# Find GTest package
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Test files
file(GLOB_RECURSE TEST_SOURCES "*.cpp")

# Create test executable
add_executable(runTests ${TEST_SOURCES})
target_link_libraries(runTests ${GTEST_LIBRARIES} pthread)

# Add tests
add_test(NAME ChessEngineTests COMMAND runTests)