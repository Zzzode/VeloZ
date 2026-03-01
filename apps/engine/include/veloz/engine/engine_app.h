#pragma once

#include "veloz/core/event_loop.h"
#include "veloz/engine/engine_config.h"
#include "veloz/engine/event_emitter.h"
#include "veloz/engine/market_data_manager.h"
#include "veloz/strategy/strategy.h"

#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/io.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/thread.h>

namespace veloz::core {
class Logger;
}

namespace veloz::engine {

class EngineHttpServer;

class EngineApp final {
public:
  EngineApp(EngineConfig config, kj::OutputStream& out, kj::OutputStream& err);

  int run();

private:
  friend struct EngineAppTestAccess;

  EngineConfig config_;
  kj::OutputStream& out_;
  kj::OutputStream& err_;
  kj::MutexGuarded<bool> stop_{false};
  kj::Maybe<veloz::core::Logger&> logger_;

  // Market data components (created in service mode)
  kj::Maybe<kj::Own<EventEmitter>> emitter_;
  kj::Maybe<kj::Own<MarketDataManager>> market_data_manager_;

  // Strategy runtime (created in service mode)
  kj::Maybe<kj::Own<veloz::strategy::StrategyManager>> strategy_manager_;

  kj::Maybe<kj::Own<veloz::core::EventLoop>> event_loop_;
  kj::Maybe<kj::Own<kj::Thread>> event_loop_thread_;

  void install_signal_handlers();
  int run_stdio();
  int run_service();

  void start_event_loop();
  void stop_event_loop();
  [[nodiscard]] bool is_event_loop_running() const;

  // Service mode helpers
  kj::Promise<void> run_http_server(kj::AsyncIoContext& io, EngineHttpServer& server);
  kj::Promise<void> run_main_loop(kj::Timer& timer);
  kj::Promise<void> run_market_data(kj::AsyncIoContext& io);
};

} // namespace veloz::engine
