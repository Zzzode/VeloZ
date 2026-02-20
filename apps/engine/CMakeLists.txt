add_library(veloz_engine_lib
  src/command_parser.cpp
  src/engine_app.cpp
  src/engine_state.cpp
  src/event_emitter.cpp
  src/http_service.cpp
  src/market_data_manager.cpp
  src/state_persistence.cpp
  src/stdio_engine.cpp
)

target_compile_features(veloz_engine_lib PRIVATE cxx_std_23)
target_include_directories(veloz_engine_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(veloz_engine_lib
  PUBLIC
    veloz_market
    veloz_exec
    veloz_oms
    veloz_risk
    veloz_strategy
    kj           # KJ core types (Vector, HashMap, Own, Maybe, String)
    kj-async     # KJ async primitives (Promise, Timer)
    kj-http      # KJ HTTP server/client
  PRIVATE
    veloz_core
    veloz_common
)

add_executable(veloz_engine
  src/main.cpp
)

target_compile_features(veloz_engine PRIVATE cxx_std_23)
target_link_libraries(veloz_engine PRIVATE veloz_engine_lib)

# Add tests
enable_testing()
add_subdirectory(tests)
