add_library(veloz_exec
  src/order_api.cpp
)

target_include_directories(veloz_exec
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(veloz_exec PUBLIC veloz_common veloz_core)
target_compile_features(veloz_exec PUBLIC cxx_std_23)

