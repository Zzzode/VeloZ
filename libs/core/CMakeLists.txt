add_library(veloz_core
    src/event_loop.cpp
    src/optimized_event_loop.cpp
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

# Build KJ library from source
# Set VERSION variable required by KJ's CMakeLists.txt
set(VERSION "2.0.0")
# Create "check" target required by KJ's CMakeLists.txt for test dependencies
if(NOT TARGET check)
  add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
endif()
add_subdirectory(kj)

target_include_directories(veloz_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against KJ library (kj-async includes async primitives like Promise, TaskSet, Timer)
target_link_libraries(veloz_core PUBLIC kj kj-async)

target_compile_features(veloz_core PUBLIC cxx_std_23)

# yyjson library
include(FetchContent)
FetchContent_Declare(
    yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG 0.9.0
)
FetchContent_MakeAvailable(yyjson)

# Try to find system libcurl first
find_package(CURL)
if(CURL_FOUND)
  target_link_libraries(veloz_core PUBLIC CURL::libcurl)
else()
  message(WARNING "libcurl not found on system - disabling curl support")
endif()

# Link against yyjson
target_link_libraries(veloz_core PUBLIC yyjson)

# Platform-specific KJ includes
if(UNIX)
  target_compile_definitions(veloz_core PRIVATE _GNU_SOURCE _POSIX_C_SOURCE=200809L)
elseif(WIN32)
  target_compile_definitions(veloz_core PRIVATE _WIN32 _CRT_SECURE_NO_WARNINGS)
endif()

# Tests
if(VELOZ_BUILD_TESTS)
  add_executable(veloz_core_tests
    tests/test_event_loop.cpp
    tests/test_memory_pool.cpp
    tests/test_logger.cpp
    tests/test_config_manager.cpp
    tests/test_json.cpp
    tests/test_retry.cpp
    tests/test_lockfree_queue.cpp
    tests/test_timer_wheel.cpp
    tests/test_optimized_event_loop.cpp
    )
  target_link_libraries(veloz_core_tests
    PRIVATE
      veloz_core
      kj-test
    )
  add_test(NAME veloz_core_tests COMMAND veloz_core_tests)
endif()

# Benchmarks
option(VELOZ_BUILD_BENCHMARKS "Build performance benchmarks" ON)
if(VELOZ_BUILD_BENCHMARKS)
  add_executable(veloz_core_benchmarks
    benchmarks/benchmark_core.cpp
    )
  target_include_directories(veloz_core_benchmarks
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks
    )
  target_link_libraries(veloz_core_benchmarks
    PRIVATE
      veloz_core
    )
  # Set Release optimization for benchmarks even in Debug builds
  target_compile_options(veloz_core_benchmarks PRIVATE
    $<$<CONFIG:Debug>:-O2>
    )
endif()
