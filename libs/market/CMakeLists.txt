cmake_minimum_required(VERSION 3.24)
project(veloz_market LANGUAGES CXX)

# Core market sources (always built)
set(MARKET_SOURCES
    src/market_event.cpp
    src/order_book.cpp
    src/subscription_manager.cpp
    src/metrics.cpp
)

# WebSocket sources (conditionally built)
# Custom WebSocket implementation using KJ async I/O (RFC 6455 protocol)
if(VELOZ_BUILD_WEBSOCKET)
    list(APPEND MARKET_SOURCES src/binance_websocket.cpp)
endif()

add_library(veloz_market STATIC ${MARKET_SOURCES})

# Include directories
target_include_directories(veloz_market
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    $<INSTALL_INTERFACE:include>
    )

target_compile_features(veloz_market PUBLIC cxx_std_23)

# Dependencies
target_link_libraries(veloz_market
    PRIVATE
        veloz_core
        veloz_common
        kj-test
    )

# Conditionally link KJ async/TLS for WebSocket support
if(VELOZ_BUILD_WEBSOCKET)
    target_link_libraries(veloz_market
        PUBLIC
            kj
            kj-async
            kj-tls
    )
    target_compile_definitions(veloz_market PUBLIC VELOZ_HAS_WEBSOCKET=1)
endif()

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
            kj-test
        )
    add_test(NAME veloz_market_tests COMMAND veloz_market_tests)

    # WebSocket unit tests (conditionally built)
    if(VELOZ_BUILD_WEBSOCKET)
        add_executable(veloz_websocket_tests
            tests/test_websocket.cpp
            )
        target_link_libraries(veloz_websocket_tests
            PRIVATE
                veloz_market
                veloz_common
                kj-test
            )
        add_test(NAME veloz_websocket_tests COMMAND veloz_websocket_tests)
    endif()
endif()
