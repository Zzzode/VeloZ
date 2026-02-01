add_library(veloz_oms
  src/order_record.cpp
)

target_include_directories(veloz_oms
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(veloz_oms PUBLIC veloz_common veloz_core veloz_exec)
target_compile_features(veloz_oms PUBLIC cxx_std_23)

