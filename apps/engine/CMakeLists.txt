add_executable(veloz_engine
  src/main.cpp
)

target_compile_features(veloz_engine PRIVATE cxx_std_23)
target_link_libraries(veloz_engine PRIVATE veloz_core veloz_common veloz_market veloz_exec veloz_oms veloz_risk)
