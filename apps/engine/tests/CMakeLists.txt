# Tests for engine module

# Find GTest
find_package(GTest REQUIRED)

# Include GTest directories
include_directories(${GTEST_INCLUDE_DIRS})

# Create test executables
add_executable(test_command_parser
    test_command_parser.cpp
)

target_link_libraries(test_command_parser
    PRIVATE
    veloz_engine_lib
    ${GTEST_LIBRARIES}
)

add_executable(test_stdio_engine
    test_stdio_engine.cpp
)

target_link_libraries(test_stdio_engine
    PRIVATE
    veloz_engine_lib
    ${GTEST_LIBRARIES}
)

# Register tests
include(GoogleTest)
gtest_discover_tests(test_command_parser)
gtest_discover_tests(test_stdio_engine)
