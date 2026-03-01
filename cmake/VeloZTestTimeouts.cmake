# VeloZTestTimeouts.cmake
#
# Unified test timeout configuration for VeloZ C++ tests.
# Provides timeout categories and helper functions for consistent test setup.
#
# Usage:
#   include(VeloZTestTimeouts)
#   add_test(NAME my_test COMMAND my_test)
#   set_tests_properties(my_test PROPERTIES TIMEOUT ${VELOZ_TEST_TIMEOUT_DEFAULT})
#
# Or use the helper function:
#   veloz_add_test(NAME my_test COMMAND my_test TIMEOUT_SHORT)

# Timeout categories (in seconds)
# Adjust these values based on your CI/CD requirements
set(VELOZ_TEST_TIMEOUT_SHORT 10 CACHE STRING "Timeout for fast unit tests (seconds)")
set(VELOZ_TEST_TIMEOUT_DEFAULT 30 CACHE STRING "Default timeout for standard tests (seconds)")
set(VELOZ_TEST_TIMEOUT_LONG 120 CACHE STRING "Timeout for integration tests and I/O tests (seconds)")
set(VELOZ_TEST_TIMEOUT_EXTENDED 300 CACHE STRING "Timeout for load tests and performance tests (seconds)")

# Label constants for test categorization
set(VELOZ_TEST_LABEL_UNIT "unit")
set(VELOZ_TEST_LABEL_INTEGRATION "integration")
set(VELOZ_TEST_LABEL_LOAD "load")
set(VELOZ_TEST_LABEL_SLOW "slow")
set(VELOZ_TEST_LABEL_NETWORK "network")

# Helper function to register a test with timeout
#
# Parameters:
#   NAME        - Test name (required)
#   COMMAND     - Test executable and arguments (required)
#   TIMEOUT     - Timeout category: SHORT, DEFAULT, LONG, or EXTENDED (default: DEFAULT)
#   LABELS      - Semicolon-separated list of labels (optional)
#   WORKING_DIRECTORY - Working directory for the test (optional)
#
# Example:
#   veloz_add_test(
#     NAME veloz_core_tests
#     COMMAND veloz_core_tests
#     TIMEOUT DEFAULT
#     LABELS "unit;core"
#   )
#
function(veloz_add_test)
  set(options "")
  set(oneValueArgs NAME TIMEOUT WORKING_DIRECTORY)
  set(multiValueArgs COMMAND LABELS)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Validate required arguments
  if(NOT ARG_NAME)
    message(FATAL_ERROR "veloz_add_test: NAME is required")
  endif()
  if(NOT ARG_COMMAND)
    message(FATAL_ERROR "veloz_add_test: COMMAND is required")
  endif()

  # Resolve timeout value
  if(ARG_TIMEOUT)
    string(TOUPPER "${ARG_TIMEOUT}" timeout_category)
    if(timeout_category STREQUAL "SHORT")
      set(timeout_value ${VELOZ_TEST_TIMEOUT_SHORT})
    elseif(timeout_category STREQUAL "DEFAULT")
      set(timeout_value ${VELOZ_TEST_TIMEOUT_DEFAULT})
    elseif(timeout_category STREQUAL "LONG")
      set(timeout_value ${VELOZ_TEST_TIMEOUT_LONG})
    elseif(timeout_category STREQUAL "EXTENDED")
      set(timeout_value ${VELOZ_TEST_TIMEOUT_EXTENDED})
    else()
      # Assume it's a numeric value
      set(timeout_value ${ARG_TIMEOUT})
    endif()
  else()
    set(timeout_value ${VELOZ_TEST_TIMEOUT_DEFAULT})
  endif()

  # Register the test
  add_test(NAME ${ARG_NAME} COMMAND ${ARG_COMMAND})

  # Set properties
  set(test_properties TIMEOUT ${timeout_value})

  if(ARG_LABELS)
    list(APPEND test_properties LABELS "${ARG_LABELS}")
  endif()

  if(ARG_WORKING_DIRECTORY)
    list(APPEND test_properties WORKING_DIRECTORY "${ARG_WORKING_DIRECTORY}")
  endif()

  set_tests_properties(${ARG_NAME} PROPERTIES ${test_properties})

  # Log for debugging
  if(CMAKE_VERBOSE_MAKEFILE)
    message(STATUS "Registered test: ${ARG_NAME} (timeout: ${timeout_value}s)")
  endif()
endfunction()

# Helper function to set timeout on existing test
#
# Parameters:
#   TEST_NAME   - Name of an existing test (required)
#   TIMEOUT     - Timeout category or numeric value (required)
#
# Example:
#   veloz_set_test_timeout(my_test LONG)
#
function(veloz_set_test_timeout TEST_NAME TIMEOUT)
  string(TOUPPER "${TIMEOUT}" timeout_category)
  if(timeout_category STREQUAL "SHORT")
    set(timeout_value ${VELOZ_TEST_TIMEOUT_SHORT})
  elseif(timeout_category STREQUAL "DEFAULT")
    set(timeout_value ${VELOZ_TEST_TIMEOUT_DEFAULT})
  elseif(timeout_category STREQUAL "LONG")
    set(timeout_value ${VELOZ_TEST_TIMEOUT_LONG})
  elseif(timeout_category STREQUAL "EXTENDED")
    set(timeout_value ${VELOZ_TEST_TIMEOUT_EXTENDED})
  else()
    set(timeout_value ${TIMEOUT})
  endif()

  set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT ${timeout_value})
endfunction()