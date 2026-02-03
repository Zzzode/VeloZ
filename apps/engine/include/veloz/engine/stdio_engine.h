#pragma once

#include "veloz/engine/engine_state.h"
#include "veloz/engine/event_emitter.h"

#include <atomic>
#include <ostream>

namespace veloz::engine {

class StdioEngine final {
public:
  explicit StdioEngine(std::ostream& out);

  int run(std::atomic<bool>& stop_flag);

private:
  EngineState state_;
  EventEmitter emitter_;
};

}
