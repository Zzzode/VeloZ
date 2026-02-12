add_library(veloz_core
  src/event_loop.cpp
  src/logger.cpp
  src/time.cpp
  src/config.cpp
  src/error.cpp
  src/memory.cpp
  src/metrics.cpp
  src/memory_pool.cpp
  src/json.cpp
  src/config_manager.cpp
)

target_include_directories(veloz_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(veloz_core PUBLIC cxx_std_23)

# yyjson library
include(FetchContent)
FetchContent_Declare(
  yyjson
  GIT_REPOSITORY https://github.com/ibireme/yyjson.git
  GIT_TAG 0.9.0
)
FetchContent_MakeAvailable(yyjson)

# websocketpp library
FetchContent_Declare(
  websocketpp
  GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
  GIT_TAG 0.8.2
)
FetchContent_MakeAvailable(websocketpp)

# Try to find system libcurl first
find_package(CURL)
if(CURL_FOUND)
  target_link_libraries(veloz_core PUBLIC CURL::libcurl)
else()
  message(WARNING "libcurl not found on system - disabling curl support")
endif()

# Link against yyjson
target_link_libraries(veloz_core PUBLIC yyjson)

# Tests
if(VELOZ_BUILD_TESTS)
  add_executable(veloz_core_tests
    tests/test_event_loop.cpp
    tests/test_memory_pool.cpp
    tests/test_logger.cpp
    tests/test_config_manager.cpp
    tests/test_json.cpp
  )
  target_link_libraries(veloz_core_tests
    PRIVATE
      veloz_core
      GTest::gtest
      GTest::gtest_main
    )
  gtest_discover_tests(veloz_core_tests)
endif()
