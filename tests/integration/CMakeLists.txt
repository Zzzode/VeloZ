cmake_minimum_required(VERSION 3.24)
project(veloz_integration_tests LANGUAGES CXX)

# Integration tests for VeloZ
# These tests verify end-to-end functionality across multiple modules

# Backtest end-to-end integration tests
add_executable(veloz_integration_backtest_tests
    test_backtest_e2e.cpp
)
target_link_libraries(veloz_integration_backtest_tests
    PRIVATE
        veloz_backtest
        veloz_strategy
        veloz_market
        veloz_core
        kj-test
        kj-async
)
target_compile_features(veloz_integration_backtest_tests PUBLIC cxx_std_23)
add_test(NAME veloz_integration_backtest_tests COMMAND veloz_integration_backtest_tests)

# Order WAL recovery integration tests
add_executable(veloz_integration_wal_tests
    test_order_wal_recovery.cpp
)
target_link_libraries(veloz_integration_wal_tests
    PRIVATE
        veloz_oms
        veloz_exec
        veloz_core
        veloz_common
        kj-test
        kj-async
)
target_compile_features(veloz_integration_wal_tests PUBLIC cxx_std_23)
add_test(NAME veloz_integration_wal_tests COMMAND veloz_integration_wal_tests)

# Multi-strategy concurrent execution integration tests
add_executable(veloz_integration_strategy_tests
    test_multi_strategy.cpp
)
target_link_libraries(veloz_integration_strategy_tests
    PRIVATE
        veloz_strategy
        veloz_market
        veloz_oms
        veloz_exec
        veloz_core
        kj-test
        kj-async
)
target_compile_features(veloz_integration_strategy_tests PUBLIC cxx_std_23)
add_test(NAME veloz_integration_strategy_tests COMMAND veloz_integration_strategy_tests)

# Engine integration tests (full event flow)
add_executable(veloz_integration_engine_tests
    test_engine_integration.cpp
)
target_link_libraries(veloz_integration_engine_tests
    PRIVATE
        veloz_strategy
        veloz_market
        veloz_oms
        veloz_exec
        veloz_core
        veloz_common
        kj-test
        kj-async
)
target_compile_features(veloz_integration_engine_tests PUBLIC cxx_std_23)
add_test(NAME veloz_integration_engine_tests COMMAND veloz_integration_engine_tests)
