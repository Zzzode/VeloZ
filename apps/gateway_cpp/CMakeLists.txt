add_library(veloz_gateway_lib
  src/gateway_server.cpp
)

target_compile_features(veloz_gateway_lib PRIVATE cxx_std_23)
target_link_libraries(veloz_gateway_lib PUBLIC kj kj-async kj-http veloz_core veloz_common)
target_include_directories(veloz_gateway_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(veloz_gateway
  src/main.cpp
)

target_compile_features(veloz_gateway PRIVATE cxx_std_23)
target_link_libraries(veloz_gateway PRIVATE veloz_gateway_lib)

if(VELOZ_BUILD_TESTS)
  add_executable(veloz_gateway_tests
    tests/test_gateway_routes.cpp
  )
  target_include_directories(veloz_gateway_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(veloz_gateway_tests PRIVATE kj-test kj veloz_gateway_lib)
  add_test(NAME veloz_gateway_tests COMMAND veloz_gateway_tests)
endif()
