add_library(veloz_common INTERFACE)

target_include_directories(veloz_common
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(veloz_common INTERFACE cxx_std_23)

