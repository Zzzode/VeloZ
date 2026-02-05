---
paths:
  - "apps/engine/**"
  - "libs/**"
  - "CMakeLists.txt"
  - "CMakePresets.json"
  - "scripts/build.sh"
  - "scripts/run_engine.sh"
---

# C++ engine / libs (module notes)

- The gateway runs the engine in **`--stdio` mode**. Keep `--stdio` behavior and the stdio wire protocol compatible when changing engine code.
- **Inbound commands (stdin)** are plain text:
  - `ORDER <BUY|SELL> <symbol> <qty> <price> <client_order_id>`
  - `CANCEL <client_order_id>`
- **Outbound events (stdout)** are **NDJSON** (one JSON object per line). The gateway/UI depend on stable `type` and field names.
  - Common `type`: `market`, `order_update`, `fill`, `order_state`, `account`, `error`.
- Key entrypoints:
  - `apps/engine/src/main.cpp` (flag parsing)
  - `apps/engine/src/engine_app.cpp` (mode switch)
  - `apps/engine/src/stdio_engine.cpp` (stdio loop)
  - `apps/engine/src/command_parser.cpp` (ORDER/CANCEL parsing)
  - `apps/engine/src/event_emitter.cpp` (event JSON emission)
- Build outputs follow CMake presets: `build/<preset>/apps/engine/veloz_engine`.
  - `./scripts/build.sh <preset>` is the canonical wrapper used by other scripts.
  - `asan` preset is available for ASan/UBSan self-check builds.
