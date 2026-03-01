# CMakeLists.txt for VeloZ C++ Gateway

cmake_minimum_required(VERSION 3.24)

# Gateway library target with all source files
add_library(veloz_gateway_lib
  # Core gateway components
  src/router.cpp
  src/gateway_server.cpp

  # Authentication module
  src/auth/jwt_manager.cpp
  src/auth/api_key_manager.cpp
  src/auth/auth_manager.cpp
  src/auth/rbac.cpp
  src/veloz/gateway/auth/rbac.cpp

  # Middleware
  src/middleware/rate_limiter.cpp
  src/middleware/auth_middleware.cpp
  src/middleware/metrics_middleware.cpp
  src/middleware/cors_middleware.cpp

  # Request handlers
  src/handlers/sse_handler.cpp
  src/handlers/market_handler.cpp
  src/handlers/health_handler.cpp
  src/handlers/auth_handler.cpp
  src/handlers/order_handler.cpp
  src/handlers/account_handler.cpp
  src/handlers/config_handler.cpp
  src/handlers/static_handler.cpp
  src/handlers/metrics_handler.cpp
  src/handlers/audit_handler.cpp

  # Static file server
  src/static/static_file_server.cpp

  # Engine bridge
  src/bridge/engine_bridge.cpp
  src/bridge/event_broadcaster.cpp
  src/bridge/event.cpp
  src/bridge/subprocess.cpp

  # Audit logging
  src/audit/audit_logger.cpp
  src/audit/audit_store.cpp
)

# C++23 standard required
target_compile_features(veloz_gateway_lib PUBLIC cxx_std_23)

# Include directories for headers
target_include_directories(veloz_gateway_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link against dependencies
# EngineBridge requires market, exec, and oms libraries for type definitions
target_link_libraries(veloz_gateway_lib PUBLIC
  kj kj-async kj-http
  veloz_core veloz_common veloz_market veloz_exec veloz_oms
  OpenSSL::SSL OpenSSL::Crypto
)

# Platform-specific compile definitions
if(UNIX)
  target_compile_definitions(veloz_gateway_lib PRIVATE
    _GNU_SOURCE
    _POSIX_C_SOURCE=200809L
  )
elseif(WIN32)
  target_compile_definitions(veloz_gateway_lib PRIVATE
    _WIN32
    _CRT_SECURE_NO_WARNINGS
  )
endif()

# Compiler warnings
target_compile_options(veloz_gateway_lib PRIVATE
  $<$<CONFIG:Debug>:-g -Wall -Wextra>
  $<$<CONFIG:Release>:-O2 -Wall -Wextra>
  $<$<CONFIG:RelWithDebInfo>:-O2 -g -Wall -Wextra>
)

# Gateway executable
add_executable(veloz_gateway src/main.cpp)
target_compile_features(veloz_gateway PRIVATE cxx_std_23)
target_link_libraries(veloz_gateway PRIVATE veloz_gateway_lib)

# Tests
if(VELOZ_BUILD_TESTS)
  enable_testing()

  add_executable(veloz_gateway_tests
    tests/test_router.cpp
    tests/test_gateway_server.cpp
    tests/test_engine_bridge.cpp
    tests/test_sse_handler.cpp
    tests/test_api_key_manager.cpp
    tests/test_jwt_manager.cpp
    tests/test_rbac.cpp
    tests/test_rate_limiter.cpp
    tests/test_market_handler.cpp
    tests/test_auth_middleware.cpp
    tests/test_metrics_middleware.cpp
    tests/test_order_handler.cpp
    tests/test_auth_handler.cpp
    tests/test_account_handler.cpp
    tests/test_config_handler.cpp
    tests/test_metrics_audit_handlers.cpp
    tests/test_health_handler.cpp
    tests/test_audit_logger.cpp
    tests/test_cors_middleware.cpp
    tests/test_sse_event.cpp
    tests/test_static_handler.cpp
    tests/test_main.cpp
  )

  target_compile_features(veloz_gateway_tests PRIVATE cxx_std_23)
  target_include_directories(veloz_gateway_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_link_libraries(veloz_gateway_tests PRIVATE
    kj-test kj kj-async kj-http
    veloz_gateway_lib
  )

  # Add test to CTest with timeout and labels
  add_test(NAME veloz_gateway_tests COMMAND veloz_gateway_tests)
  set_tests_properties(veloz_gateway_tests PROPERTIES
    TIMEOUT ${VELOZ_TEST_TIMEOUT_DEFAULT}
    LABELS "unit;gateway"
  )

  # Compiler warnings for tests
  target_compile_options(veloz_gateway_tests PRIVATE
    $<$<CONFIG:Debug>:-g -Wall -Wextra>
    $<$<CONFIG:Release>:-O2 -Wall -Wextra>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g -Wall -Wextra>
  )

  # Integration tests (end-to-end testing)
  add_executable(veloz_gateway_integration_tests
    tests/integration/test_full_flow.cpp
    tests/integration/test_rate_limiting_integration.cpp
    tests/integration/test_sse_integration.cpp
    tests/integration/test_concurrent_access.cpp
  )

  target_compile_features(veloz_gateway_integration_tests PRIVATE cxx_std_23)
  target_include_directories(veloz_gateway_integration_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration
  )
  target_link_libraries(veloz_gateway_integration_tests PRIVATE
    kj-test kj kj-async kj-http
    veloz_gateway_lib
    pthread  # For std::thread in concurrent tests
  )

  # Add integration test to CTest with timeout and labels
  add_test(NAME veloz_gateway_integration_tests COMMAND veloz_gateway_integration_tests)
  set_tests_properties(veloz_gateway_integration_tests PROPERTIES
    TIMEOUT ${VELOZ_TEST_TIMEOUT_EXTENDED}
    LABELS "integration;gateway;slow"
    RUN_SERIAL TRUE
  )

  # Compiler warnings for integration tests
  target_compile_options(veloz_gateway_integration_tests PRIVATE
    $<$<CONFIG:Debug>:-g -Wall -Wextra>
    $<$<CONFIG:Release>:-O2 -Wall -Wextra>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g -Wall -Wextra>
  )
endif()

# Benchmarks
option(VELOZ_BUILD_GATEWAY_BENCHMARKS "Build gateway performance benchmarks" ON)
if(VELOZ_BUILD_GATEWAY_BENCHMARKS)
  add_executable(veloz_gateway_benchmarks
    benchmarks/benchmark_gateway.cpp
  )
  target_compile_features(veloz_gateway_benchmarks PRIVATE cxx_std_23)
  target_include_directories(veloz_gateway_benchmarks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_link_libraries(veloz_gateway_benchmarks PRIVATE
    veloz_gateway_lib
  )
  # Use Release optimization for benchmarks
  target_compile_options(veloz_gateway_benchmarks PRIVATE
    $<$<CONFIG:Debug>:-O2>
    $<$<CONFIG:Release>:-O3>
  )

  # Add benchmark run target
  add_custom_target(run_gateway_benchmarks
    COMMAND veloz_gateway_benchmarks
    DEPENDS veloz_gateway_benchmarks
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running gateway performance benchmarks"
  )

  # Add benchmark report generation target
  find_package(Python3 QUIET)
  if(Python3_FOUND)
    add_custom_target(benchmark_report
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/benchmark_report
      COMMAND veloz_gateway_benchmarks > ${CMAKE_CURRENT_BINARY_DIR}/benchmark_output.txt
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/benchmark_report.py
        --input ${CMAKE_CURRENT_BINARY_DIR}/benchmark_output.txt
        --output ${CMAKE_CURRENT_BINARY_DIR}/benchmark_report
      DEPENDS veloz_gateway_benchmarks
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Generating benchmark report"
    )
  endif()
endif()
