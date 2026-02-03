#pragma once

#include "veloz/engine/engine_config.h"

#include <atomic>
#include <ostream>

namespace veloz::core {
class Logger;
}

namespace veloz::engine {

class EngineApp final {
public:
  EngineApp(EngineConfig config, std::ostream& out, std::ostream& err);

  int run();

private:
  EngineConfig config_;
  std::ostream& out_;
  std::ostream& err_;
  std::atomic<bool> stop_{false};
  veloz::core::Logger* logger_{nullptr};

  void install_signal_handlers();
  int run_stdio();
  int run_service();
};

}
