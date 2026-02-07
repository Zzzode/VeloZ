cmake_minimum_required(VERSION 3.24)

project(VeloZ LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VELOZ_ENABLE_WERROR "Treat warnings as errors" OFF)
option(VELOZ_BUILD_TESTS "Build tests" ON)

# GoogleTest setup
if(VELOZ_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  enable_testing()
  include(GoogleTest)
endif()

if (MSVC)
  add_compile_options(/W4)
  if (VELOZ_ENABLE_WERROR)
    add_compile_options(/WX)
  endif()
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  if (VELOZ_ENABLE_WERROR)
    add_compile_options(-Werror)
  endif()
endif()

add_subdirectory(libs/common)
add_subdirectory(libs/core)
add_subdirectory(libs/market)
add_subdirectory(libs/exec)
add_subdirectory(libs/oms)
add_subdirectory(libs/risk)
add_subdirectory(apps/engine)
