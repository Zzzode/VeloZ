cmake_minimum_required(VERSION 3.24)
project(veloz_market LANGUAGES CXX)

add_library(veloz_market STATIC
    src/market_event.cpp
    src/order_book.cpp
    src/subscription_manager.cpp
    src/metrics.cpp
    src/binance_websocket.cpp
)

# Include directories
target_include_directories(veloz_market
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${websocketpp_SOURCE_DIR}
    $<INSTALL_INTERFACE:include>
)

target_compile_features(veloz_market PUBLIC cxx_std_23)

# Dependencies
target_link_libraries(veloz_market
    PRIVATE
        veloz_core
        veloz_common
        GTest::gtest
        GTest::gtest_main)

# Tests
if(VELOZ_BUILD_TESTS)
    add_executable(veloz_market_tests
        tests/test_market_event.cpp
        tests/test_order_book.cpp
        tests/test_subscription_manager.cpp
        tests/test_metrics.cpp
        )
    target_link_libraries(veloz_market_tests
        PRIVATE
            veloz_market
            veloz_common
            GTest::gtest
            GTest::gtest_main
    )
    gtest_discover_tests(veloz_market_tests)
endif()
