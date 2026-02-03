add_executable(veloz_engine
  src/main.cpp
  src/command_parser.cpp
  src/engine_app.cpp
  src/engine_state.cpp
  src/event_emitter.cpp
  src/stdio_engine.cpp
)

target_compile_features(veloz_engine PRIVATE cxx_std_23)
target_include_directories(veloz_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(veloz_engine PRIVATE veloz_core veloz_common veloz_market veloz_exec veloz_oms veloz_risk)
