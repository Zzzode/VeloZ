cmake_minimum_required(VERSION 3.24)

add_library(veloz_backtest STATIC)

# Include directories
target_include_directories(veloz_backtest
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Source files
target_sources(veloz_backtest
    PRIVATE
        src/backtest_engine.cpp
        src/analyzer.cpp
        src/optimizer.cpp
        src/reporter.cpp
        src/data_source.cpp
)

# Header files (for installation)
set_target_properties(veloz_backtest PROPERTIES
    PUBLIC_HEADER "include/veloz/backtest/types.h;include/veloz/backtest/backtest_engine.h;include/veloz/backtest/analyzer.h;include/veloz/backtest/optimizer.h;include/veloz/backtest/reporter.h;include/veloz/backtest/data_source.h"
)

# Link dependencies
target_link_libraries(veloz_backtest
    PRIVATE
        veloz_core
        veloz_strategy
        veloz_market
        veloz_oms
        veloz_exec
        veloz_risk
        veloz_common
)

# Link gtest only if tests are enabled
if(VELOZ_BUILD_TESTS)
    target_link_libraries(veloz_backtest
        PRIVATE
            GTest::gtest
            GTest::gtest_main
    )
endif()

# Compiler warnings and flags
if (MSVC)
    target_compile_options(veloz_backtest PRIVATE /W4)
else()
    target_compile_options(veloz_backtest PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Tests
if(VELOZ_BUILD_TESTS)
    add_executable(veloz_backtest_tests
        tests/test_backtest_engine.cpp
        tests/test_analyzer.cpp
        tests/test_optimizer.cpp
        tests/test_reporter.cpp
        tests/test_data_source.cpp
    )

    target_link_libraries(veloz_backtest_tests
        PRIVATE
            veloz_backtest
            GTest::gtest
            GTest::gtest_main
    )

    target_include_directories(veloz_backtest_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    # Register tests
    gtest_discover_tests(veloz_backtest_tests)
endif()
