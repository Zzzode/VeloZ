# Tests for engine module using KJ test framework
# KJ Test is provided by libs/core/kj/CMakeLists.txt

# Create test executables
add_executable(test_command_parser
    test_command_parser.cpp
)

target_link_libraries(test_command_parser
    PRIVATE
    veloz_engine_lib
    kj-test
    kj-async
)

add_executable(test_stdio_engine
    test_stdio_engine.cpp
)

target_link_libraries(test_stdio_engine
    PRIVATE
    veloz_engine_lib
    kj-test
    kj-async
)

# HTTP service tests
add_executable(test_http_service
    test_http_service.cpp
)

target_link_libraries(test_http_service
    PRIVATE
    veloz_engine_lib
    kj-test
    kj-async
)

add_executable(test_engine_app
    test_engine_app.cpp
)

target_link_libraries(test_engine_app
    PRIVATE
    veloz_engine_lib
    kj-test
    kj-async
)

# Register tests with timeout and labels
add_test(NAME test_command_parser COMMAND test_command_parser)
add_test(NAME test_stdio_engine COMMAND test_stdio_engine)
add_test(NAME test_http_service COMMAND test_http_service)
add_test(NAME test_engine_app COMMAND test_engine_app)

set_tests_properties(
    test_command_parser
    test_stdio_engine
    test_http_service
    test_engine_app
    PROPERTIES
    TIMEOUT ${VELOZ_TEST_TIMEOUT_DEFAULT}
    LABELS "unit;engine"
)

# State persistence tests
add_executable(test_state_persistence
    test_state_persistence.cpp
)

target_link_libraries(test_state_persistence
    PRIVATE
    veloz_engine_lib
    kj-test
    kj-async
)

add_test(NAME test_state_persistence COMMAND test_state_persistence)
set_tests_properties(test_state_persistence PROPERTIES
    TIMEOUT ${VELOZ_TEST_TIMEOUT_DEFAULT}
    LABELS "unit;engine"
)
