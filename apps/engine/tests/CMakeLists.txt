# Tests for engine module
# GTest is provided via FetchContent in root CMakeLists.txt

# Create test executables
add_executable(test_command_parser
    test_command_parser.cpp
)

target_link_libraries(test_command_parser
    PRIVATE
    veloz_engine_lib
    GTest::gtest
    GTest::gtest_main
)

add_executable(test_stdio_engine
    test_stdio_engine.cpp
)

target_link_libraries(test_stdio_engine
    PRIVATE
    veloz_engine_lib
    GTest::gtest
    GTest::gtest_main
)

# Register tests
gtest_discover_tests(test_command_parser)
gtest_discover_tests(test_stdio_engine)
