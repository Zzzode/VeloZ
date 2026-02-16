add_library(veloz_oms
  src/order_record.cpp
  src/order_wal.cpp
  src/position.cpp
)

target_include_directories(veloz_oms
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(veloz_oms PUBLIC veloz_common veloz_core veloz_exec)
target_compile_features(veloz_oms PUBLIC cxx_std_23)

# Tests using KJ test framework
if(VELOZ_BUILD_TESTS)
  add_executable(veloz_oms_tests
    tests/test_order_wal.cpp
    tests/test_position.cpp
  )

  target_link_libraries(veloz_oms_tests
    PRIVATE
      veloz_oms
      kj-test
  )
  add_test(NAME veloz_oms_tests COMMAND veloz_oms_tests)
endif()

