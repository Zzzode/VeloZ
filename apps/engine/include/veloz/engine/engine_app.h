#pragma once

#include "veloz/engine/engine_config.h"

#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/io.h>
#include <kj/mutex.h>

namespace veloz::core {
class Logger;
}

namespace veloz::engine {

class EngineApp final {
public:
  EngineApp(EngineConfig config, kj::OutputStream& out, kj::OutputStream& err);

  int run();

private:
  EngineConfig config_;
  kj::OutputStream& out_;
  kj::OutputStream& err_;
  kj::MutexGuarded<bool> stop_{false};
  kj::Maybe<veloz::core::Logger&> logger_;

  void install_signal_handlers();
  int run_stdio();
  int run_service();
};

} // namespace veloz::engine
