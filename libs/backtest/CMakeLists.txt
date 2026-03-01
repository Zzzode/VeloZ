cmake_minimum_required(VERSION 3.24)
project(veloz_backtest LANGUAGES CXX)

add_library(veloz_backtest STATIC
    src/backtest_engine.cpp
    src/data_source.cpp
    src/analyzer.cpp
    src/optimizer.cpp
    src/reporter.cpp
)
# Include directories
target_include_directories(veloz_backtest
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(veloz_backtest PUBLIC cxx_std_23)

# Dependencies
# KJ library is used directly for kj::Vector, kj::HashMap, kj::Own, kj::Maybe etc.
target_link_libraries(veloz_backtest
    PUBLIC
        veloz_strategy
        veloz_market
        kj           # KJ core types (Vector, HashMap, Own, Maybe, String)
        kj-async     # KJ async primitives (Promise, Timer)
    PRIVATE
        veloz_core
        veloz_oms
        veloz_exec
        veloz_risk
        veloz_common)

# Tests using KJ test framework
if(VELOZ_BUILD_TESTS)
    add_executable(veloz_backtest_tests
        tests/test_backtest_engine.cpp
        tests/test_analyzer.cpp
        tests/test_optimizer.cpp
        tests/test_reporter.cpp
        )
    target_link_libraries(veloz_backtest_tests
        PRIVATE
            veloz_backtest
            kj-test
            kj-async
    )
    add_test(NAME veloz_backtest_tests COMMAND veloz_backtest_tests)
    set_tests_properties(veloz_backtest_tests PROPERTIES
        TIMEOUT ${VELOZ_TEST_TIMEOUT_DEFAULT}
        LABELS "unit;backtest"
    )
endif()
