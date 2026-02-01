add_library(veloz_risk
  src/risk_engine.cpp
)

target_include_directories(veloz_risk
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(veloz_risk PUBLIC veloz_common veloz_core veloz_exec)
target_compile_features(veloz_risk PUBLIC cxx_std_23)

