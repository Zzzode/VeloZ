cmake_minimum_required(VERSION 3.24)

project(VeloZ LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set CMAKE_POLICY_VERSION_MINIMUM to suppress capnproto warnings
# Cap'n Proto requires CMake 3.5+ but uses older patterns
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

option(VELOZ_ENABLE_WERROR "Treat warnings as errors" OFF)
option(VELOZ_BUILD_TESTS "Build tests" ON)
option(VELOZ_BUILD_BACKTEST "Build backtest module" ON)
option(VELOZ_BUILD_WEBSOCKET "Build WebSocket market data module (uses KJ async I/O)" ON)

# Find OpenSSL for TLS support (required by WebSocket module)
find_package(OpenSSL 1.1.0 REQUIRED)

# Set WITH_OPENSSL for conditional kj-tls linking in subdirectories
if(OpenSSL_FOUND)
    set(WITH_OPENSSL ON)
endif()

# Enable testing early so BUILD_TESTING is set for all subdirectories
if(VELOZ_BUILD_TESTS)
    enable_testing()
endif()

# KJ Test framework is provided by libs/core/kj/CMakeLists.txt
# All tests use KJ Test (kj::TestRunner) instead of GoogleTest

add_subdirectory(libs/common)
add_subdirectory(libs/core)
add_subdirectory(libs/market)
add_subdirectory(libs/exec)
add_subdirectory(libs/oms)
add_subdirectory(libs/risk)
add_subdirectory(libs/strategy)
if(VELOZ_BUILD_BACKTEST)
    add_subdirectory(libs/backtest)
endif()
add_subdirectory(apps/engine)

# Integration tests
if(VELOZ_BUILD_TESTS AND VELOZ_BUILD_BACKTEST)
    add_subdirectory(tests/integration)
endif()
