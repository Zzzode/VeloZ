add_library(veloz_exec
  src/order_api.cpp
  src/order_router.cpp
  src/client_order_id.cpp
)

target_include_directories(veloz_exec
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(veloz_exec PUBLIC veloz_common veloz_core)
target_compile_features(veloz_exec PUBLIC cxx_std_23)

# Tests
if(VELOZ_BUILD_TESTS)
  add_executable(veloz_exec_tests
    tests/test_exchange_adapter_interface.cpp
    tests/test_order_router.cpp
    tests/test_client_order_id.cpp
  )

  target_link_libraries(veloz_exec_tests
    PRIVATE
      veloz_exec
      GTest::gtest
      GTest::gtest_main
  )

  gtest_discover_tests(veloz_exec_tests)
endif()

