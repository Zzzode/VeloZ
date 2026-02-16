add_library(veloz_risk
  src/risk_engine.cpp
  src/circuit_breaker.cpp
  src/risk_metrics.cpp
)

target_include_directories(veloz_risk
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(veloz_risk PUBLIC veloz_common veloz_core veloz_exec veloz_oms)
target_compile_features(veloz_risk PUBLIC cxx_std_23)

# Tests using KJ test framework
if(VELOZ_BUILD_TESTS)
  add_executable(veloz_risk_tests
    tests/test_risk_engine.cpp
    tests/test_circuit_breaker.cpp
  )

  target_link_libraries(veloz_risk_tests
    PRIVATE
      veloz_risk
      veloz_exec
      veloz_oms
      kj-test
  )
  add_test(NAME veloz_risk_tests COMMAND veloz_risk_tests)
endif()

