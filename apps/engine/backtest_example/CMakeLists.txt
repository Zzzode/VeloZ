cmake_minimum_required(VERSION 3.24)

# Backtest example
add_executable(backtest_example
    ${CMAKE_CURRENT_SOURCE_DIR}/../../backtest_example.cpp
)

# Link dependencies
target_link_libraries(backtest_example
    PRIVATE
        veloz_backtest
        veloz_core
        veloz_strategy
        veloz_market
        veloz_oms
        veloz_exec
        veloz_risk
        veloz_common
)

# Include directories
target_include_directories(backtest_example
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/backtest/include
)

# Compiler warnings and flags
if (MSVC)
    target_compile_options(backtest_example PRIVATE /W4)
else()
    target_compile_options(backtest_example PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install
install(TARGETS backtest_example
    RUNTIME DESTINATION bin
)
