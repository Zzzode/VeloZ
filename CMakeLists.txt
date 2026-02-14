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

# Enable testing early so BUILD_TESTING is set for all subdirectories
if(VELOZ_BUILD_TESTS)
    enable_testing()
endif()

# GoogleTest setup
if(VELOZ_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # For Windows: Prevent overriding parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    include(GoogleTest)
endif()

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
