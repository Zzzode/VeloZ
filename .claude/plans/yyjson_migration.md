# yyjson Migration Plan

## Context
Migrate VeloZ from nlohmann_json to yyjson for better performance and MIT licensing.

## Analysis

### Current State
- `libs/core/src/json.cpp` and `libs/core/include/veloz/core/json.h` already contain yyjson wrapper implementation
- `libs/core/CMakeLists.txt` now uses yyjson instead of nlohmann_json
- `libs/exec/CMakeLists.txt` updated to link veloz_core
- `libs/backtest/CMakeLists.txt` updated to link veloz_core
- Following files use `JsonDocument`/`JsonValue`/`JsonBuilder` (yyjson wrapper):
  - `libs/exec/src/binance_adapter.cpp` - uses JsonDocument for API responses
  - `libs/backtest/src/data_source.cpp` - uses JsonDocument for parsing
  - `libs/market/src/binance_websocket.cpp` - uses JsonDocument for WebSocket messages
  - `libs/core/src/config_manager.cpp` - uses JsonBuilder for serialization

## Migration Strategy

1. **Replace nlohmann_json with yyjson in CMakeLists.txt**
   - Done in libs/core/CMakeLists.txt
   - Done in libs/exec/CMakeLists.txt
   - Done in libs/backtest/CMakeLists.txt
   - Done in libs/market/CMakeLists.txt

2. **Keep existing json.cpp implementation**
   - The existing json.cpp provides a correct yyjson wrapper
   - No changes needed to the wrapper layer

3. **Verify source files use correct yyjson wrapper**
   - Verify they use `JsonDocument::parse()`, `JsonBuilder::object()`, etc.
   - Most files already use the correct yyjson wrapper

4. **Build and verify**
   - Run cmake configure and build
   - Run tests to ensure everything works

## Files to Verify (may need fixes):

### Source files that should use yyjson wrapper:
- `libs/exec/src/binance_adapter.cpp` - verify uses `JsonDocument::parse()`
- `libs/backtest/src/data_source.cpp` - verify uses `JsonDocument::parse()`
- `libs/market/src/binance_websocket.cpp` - verify uses `JsonDocument::parse()`
- `libs/core/src/config_manager.cpp` - verify uses `JsonBuilder` and helper functions

## Implementation Steps

1. ~~Update `libs/core/CMakeLists.txt`~~ (DONE)
2. ~~Update `libs/exec/CMakeLists.txt`~~ (DONE)
3. ~~Update `libs/backtest/CMakeLists.txt`~~ (DONE)
4. ~~Update `libs/market/CMakeLists.txt`~~ (DONE)
5. Configure and build
6. Run tests

## Verification
- Run `cmake --preset dev --build dev-all`
- Run `ctest --preset dev`
