# Tests for execution module

# Include GTest
find_package(GTest REQUIRED)

# Create test executables
add_executable(veloz_exec_tests
    test_client_order_id.cpp
    test_exchange_adapter_interface.cpp
    test_order_router.cpp
    test_binance_adapter.cpp
)

target_link_libraries(veloz_exec_tests
    PRIVATE
    veloz_exec
    veloz_common
    ${GTEST_LIBRARIES}
)

# Register tests
include(GoogleTest)
gtest_discover_tests(veloz_exec_tests)
