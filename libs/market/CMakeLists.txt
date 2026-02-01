add_library(veloz_market
  src/market_event.cpp
)

target_include_directories(veloz_market
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(veloz_market PUBLIC veloz_common veloz_core)
target_compile_features(veloz_market PUBLIC cxx_std_23)

