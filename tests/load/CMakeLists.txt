# Load Testing Framework for VeloZ
#
# This module provides comprehensive load testing capabilities:
# - Market data throughput testing (100k+ events/sec)
# - Order placement latency testing
# - Sustained load testing for memory leak detection
# - Performance validation against SLO targets

cmake_minimum_required(VERSION 3.24)
project(veloz_load_tests LANGUAGES CXX)

# Load test executable
add_executable(veloz_load_tests
    load_test_main.cpp
)

target_link_libraries(veloz_load_tests
    PRIVATE
        veloz_common
        veloz_core
        veloz_market
        veloz_exec
        veloz_oms
        kj
        kj-async
)

target_compile_features(veloz_load_tests PUBLIC cxx_std_23)

# Set output directory
set_target_properties(veloz_load_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/load"
)

# Add to CTest
add_test(
    NAME load_test_quick
    COMMAND veloz_load_tests quick
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Set timeout for quick test using project timeout variable
set_tests_properties(load_test_quick PROPERTIES
    TIMEOUT ${VELOZ_TEST_TIMEOUT_LONG}
    LABELS "load;performance"
    RUN_SERIAL TRUE
)

# Production load test executable
add_executable(veloz_production_load_test
    production_load_test.cpp
)

target_link_libraries(veloz_production_load_test
    PRIVATE
        veloz_common
        veloz_core
        veloz_market
        veloz_exec
        veloz_oms
        kj
        kj-async
)

target_compile_features(veloz_production_load_test PUBLIC cxx_std_23)

set_target_properties(veloz_production_load_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/load"
)

# Add quick production test to CTest
add_test(
    NAME production_load_test_quick
    COMMAND veloz_production_load_test --quick
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

set_tests_properties(production_load_test_quick PROPERTIES
    TIMEOUT ${VELOZ_TEST_TIMEOUT_EXTENDED}
    LABELS "load;performance;production"
    RUN_SERIAL TRUE
)

if(NOT DEFINED ENV{VELOZ_RUN_PRODUCTION_LOAD_TESTS})
  set_tests_properties(production_load_test_quick PROPERTIES DISABLED TRUE)
endif()
