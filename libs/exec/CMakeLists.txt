add_library(veloz_exec
    src/order_api.cpp
    src/order_router.cpp
    src/client_order_id.cpp
    src/binance_adapter.cpp
    src/okx_adapter.cpp
    src/coinbase_adapter.cpp
    src/bybit_adapter.cpp
    src/resilient_adapter.cpp
    src/hmac_wrapper.cpp
    src/reconciliation.cpp
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
        veloz_oms    # For OrderStore in reconciliation
        veloz_risk   # For circuit breaker in resilient adapter
        kj-test
        kj-http      # KJ HTTP client for async HTTP operations
        kj-async     # KJ async I/O
    )

# KJ TLS for HTTPS support (requires OpenSSL)
if(WITH_OPENSSL)
    target_link_libraries(veloz_exec PRIVATE kj-tls)
endif()

target_link_libraries(veloz_exec PUBLIC OpenSSL::Crypto OpenSSL::SSL)

# Tests
if(VELOZ_BUILD_TESTS)
    add_executable(veloz_exec_tests
        tests/test_exchange_adapter_interface.cpp
        tests/test_order_router.cpp
        tests/test_client_order_id.cpp
        tests/test_binance_adapter.cpp
        tests/test_exchange_adapters.cpp
        tests/test_resilient_adapter.cpp
        tests/test_reconciliation.cpp
    )
    target_link_libraries(veloz_exec_tests
        PRIVATE
            veloz_exec
            veloz_common
            veloz_oms
            veloz_risk
            kj-test
            kj-async
        )
    if(WITH_OPENSSL)
        target_link_libraries(veloz_exec_tests PRIVATE kj-tls)
    endif()
    add_test(NAME veloz_exec_tests COMMAND veloz_exec_tests)
endif()
