add_library(veloz_oms
  src/order_record.cpp
  src/position.cpp
)

target_include_directories(veloz_oms
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(veloz_oms PUBLIC veloz_common veloz_core veloz_exec)
target_compile_features(veloz_oms PUBLIC cxx_std_23)

# Tests
if(VELOZ_BUILD_TESTS)
  add_executable(veloz_oms_tests
    tests/test_position.cpp
  )

  target_link_libraries(veloz_oms_tests
    PRIVATE
      veloz_oms
      GTest::gtest
      GTest::gtest_main
  )

  gtest_discover_tests(veloz_oms_tests)
endif()

