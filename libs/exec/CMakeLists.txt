add_library(veloz_exec
    src/order_api.cpp
    src/order_router.cpp
    src/client_order_id.cpp
    src/binance_adapter.cpp
    )

target_include_directories(veloz_exec
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_compile_features(veloz_exec PUBLIC cxx_std_23)

# Link to dependencies
# KJ libraries are used for async HTTP operations (replacing CURL)
target_link_libraries(veloz_exec
    PRIVATE
        veloz_common
        veloz_core
        kj-test
        kj-http      # KJ HTTP client for async HTTP operations
        kj-tls       # KJ TLS for HTTPS support
        kj-async     # KJ async I/O
    )

# OpenSSL is still required for HMAC-SHA256 signature generation
# This is needed for Binance API authentication - KJ does not provide HMAC functionality
find_package(OpenSSL)
if(OpenSSL_FOUND)
    target_link_libraries(veloz_exec PUBLIC OpenSSL::Crypto OpenSSL::SSL)
else()
    message(WARNING "OpenSSL not found on system - disabling OpenSSL support for HMAC signatures")
    target_compile_definitions(veloz_exec PRIVATE VELOZ_NO_OPENSSL)
endif()

# Tests
if(VELOZ_BUILD_TESTS)
    add_executable(veloz_exec_tests
        tests/test_exchange_adapter_interface.cpp
        tests/test_order_router.cpp
        tests/test_client_order_id.cpp
        tests/test_binance_adapter.cpp
    )
    target_link_libraries(veloz_exec_tests
        PRIVATE
            veloz_exec
            veloz_common
            kj-test
            kj-async
        )
    add_test(NAME veloz_exec_tests COMMAND veloz_exec_tests)
endif()
