add_library(veloz_core
  src/event_loop.cpp
  src/logger.cpp
  src/time.cpp
)

target_include_directories(veloz_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(veloz_core PUBLIC cxx_std_23)

