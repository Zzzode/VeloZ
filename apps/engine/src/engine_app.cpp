#include "veloz/engine/engine_app.h"

#include "veloz/core/event_loop.h"
#include "veloz/core/logger.h"
#include "veloz/engine/http_service.h"
#include "veloz/engine/market_data_manager.h"
#include "veloz/engine/stdio_engine.h"
#include "veloz/strategy/advanced_strategies.h"
#include "veloz/strategy/strategy.h"

// std::signal for POSIX signal handling (standard C library, KJ lacks signal API)
#include <csignal>
#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/debug.h>
#include <kj/io.h>
#include <kj/memory.h>
#include <kj/time.h>
#include <unistd.h>

namespace {

// Non-owning raw pointer to EngineApp::stop_ for signal handler access.
// Raw pointer is acceptable here because:
// 1. Signal handlers require async-signal-safe code; complex types like kj::Maybe are not allowed
// 2. The pointer is set in install_signal_handlers() and points to EngineApp::stop_
// 3. Lifetime is guaranteed: stop_ outlives signal handler registration (cleared on EngineApp
// destruction)
// 4. Only accessed atomically via store() in handle_signal()
kj::MutexGuarded<bool>* g_stop_ptr = nullptr;

void handle_signal(int) {
  if (g_stop_ptr) {
    *g_stop_ptr->lockExclusive() = true;
  }
}

} // namespace

namespace veloz::engine {

EngineApp::EngineApp(EngineConfig config, kj::OutputStream& out, kj::OutputStream& err)
    : config_(config), out_(out), err_(err) {}

void EngineApp::install_signal_handlers() {
  g_stop_ptr = &stop_;
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
}

int EngineApp::run() {
  install_signal_handlers();

  // Create logger with appropriate output using KJ ownership
  auto console_output = kj::heap<veloz::core::ConsoleOutput>(config_.stdio_mode);
  veloz::core::Logger local_logger(kj::heap<veloz::core::TextFormatter>(), kj::mv(console_output));
  local_logger.set_level(veloz::core::LogLevel::Info);

  // Store logger reference for potential use
  logger_ = local_logger;

  local_logger.info(config_.stdio_mode ? "VeloZ engine starting (stdio)" : "VeloZ engine starting");

  if (config_.stdio_mode) {
    return run_stdio();
  }
  return run_service();
}

int EngineApp::run_stdio() {
  kj::FdInputStream stdinStream(STDIN_FILENO);
  StdioEngine engine(out_, stdinStream);
  const int rc = engine.run(stop_);
  return rc;
}

int EngineApp::run_service() {
  // Use KJ async I/O for the service mode
  auto io = kj::setupAsyncIo();
  auto& timer = io.provider->getTimer();
  auto& network = io.provider->getNetwork();

  KJ_IF_SOME(l, logger_) {
    l.info(kj::str("Starting service mode on port ", config_.http_port).cStr());
  }

  // Create HTTP header table
  auto headerTable = kj::heap<kj::HttpHeaderTable>();

  // Create HTTP service
  auto httpService = kj::heap<EngineHttpService>(*headerTable, stop_);

  // Create and configure strategy manager
  strategy_manager_ = kj::heap<veloz::strategy::StrategyManager>();
  KJ_IF_SOME(sm, strategy_manager_) {
    // Register built-in strategy factories
    sm->register_strategy_factory(kj::rc<veloz::strategy::RsiStrategyFactory>());
    sm->register_strategy_factory(kj::rc<veloz::strategy::MacdStrategyFactory>());
    sm->register_strategy_factory(kj::rc<veloz::strategy::BollingerBandsStrategyFactory>());
    sm->register_strategy_factory(kj::rc<veloz::strategy::StochasticOscillatorStrategyFactory>());
    sm->register_strategy_factory(kj::rc<veloz::strategy::MarketMakingHFTStrategyFactory>());
    sm->register_strategy_factory(kj::rc<veloz::strategy::CrossExchangeArbitrageStrategyFactory>());

    // Set strategy manager on HTTP service for API access
    httpService->set_strategy_manager(sm.get());

    KJ_IF_SOME(l, logger_) {
      l.info("Strategy manager initialized with built-in factories");
    }
  }

  // Set up stop callback to trigger graceful shutdown
  httpService->set_stop_callback([this]() -> bool {
    *stop_.lockExclusive() = true;
    return true;
  });

  // Set up start callback
  httpService->set_start_callback([&httpService]() -> bool {
    httpService->set_engine_state(EngineLifecycleState::Running);
    return true;
  });

  // Create HTTP server settings
  kj::HttpServerSettings settings;
  settings.headerTimeout = 30 * kj::SECONDS;
  settings.pipelineTimeout = 5 * kj::SECONDS;

  // Create HTTP server
  auto httpServer = kj::heap<kj::HttpServer>(timer, *headerTable, *httpService, settings);

  // Set engine state to running
  httpService->set_engine_state(EngineLifecycleState::Running);

  KJ_IF_SOME(l, logger_) {
    l.info(kj::str("HTTP server listening on port ", config_.http_port).cStr());
  }

  // Parse address and start listening
  auto listenPromise =
      network.parseAddress(kj::str("0.0.0.0:", config_.http_port))
          .then([&httpServer](kj::Own<kj::NetworkAddress> addr) {
            auto receiver = addr->listen();
            return httpServer->listenHttp(*receiver).attach(kj::mv(receiver), kj::mv(addr));
          });

  // Main loop - check stop flag periodically
  auto mainLoopPromise = run_main_loop(timer);

  // Start market data manager if enabled
  kj::Promise<void> marketDataPromise = kj::READY_NOW;
  if (config_.enable_market_data) {
    start_event_loop();
    KJ_IF_SOME(l, logger_) {
      l.info("Starting market data integration...");
    }
    marketDataPromise = run_market_data(io);
  }

  // Wait for either the main loop to complete (stop flag set) or listen to fail
  auto combinedPromise =
      mainLoopPromise.exclusiveJoin(kj::mv(listenPromise)).exclusiveJoin(kj::mv(marketDataPromise));

  try {
    combinedPromise.wait(io.waitScope);
  } catch (const kj::Exception& e) {
    KJ_IF_SOME(l, logger_) {
      l.error(kj::str("Service error: ", e.getDescription()).cStr());
    }
    return 1;
  }

  // Graceful shutdown
  KJ_IF_SOME(l, logger_) {
    l.info("Draining HTTP server...");
  }

  httpService->set_engine_state(EngineLifecycleState::Stopping);

  // Stop market data manager
  KJ_IF_SOME(mdm, market_data_manager_) {
    KJ_IF_SOME(l, logger_) {
      l.info("Stopping market data manager...");
    }
    mdm->stop();
  }

  stop_event_loop();

  httpServer->drain().wait(io.waitScope);

  httpService->set_engine_state(EngineLifecycleState::Stopped);

  KJ_IF_SOME(l, logger_) {
    l.info("Service mode stopped");
  }

  return 0;
}

void EngineApp::start_event_loop() {
  if (event_loop_ != kj::none) {
    return;
  }
  auto loop = kj::heap<veloz::core::EventLoop>();
  auto* loop_ptr = loop.get();
  auto thread = kj::heap<kj::Thread>([loop_ptr] { loop_ptr->run(); });
  event_loop_ = kj::mv(loop);
  event_loop_thread_ = kj::mv(thread);
}

void EngineApp::stop_event_loop() {
  KJ_IF_SOME(loop, event_loop_) {
    loop->stop();
  }
  event_loop_thread_ = kj::none;
  event_loop_ = kj::none;
}

bool EngineApp::is_event_loop_running() const {
  KJ_IF_SOME(loop, event_loop_) {
    return loop->is_running();
  }
  return false;
}

kj::Promise<void> EngineApp::run_http_server(kj::AsyncIoContext& io, EngineHttpServer& server) {
  return server.listen(io.provider->getNetwork());
}

kj::Promise<void> EngineApp::run_main_loop(kj::Timer& timer) {
  // Poll stop flag periodically
  while (!*stop_.lockShared()) {
    co_await timer.afterDelay(100 * kj::MILLISECONDS);
  }
}

kj::Promise<void> EngineApp::run_market_data(kj::AsyncIoContext& io) {
  // Create event emitter for market data output
  emitter_ = kj::heap<EventEmitter>(out_);

  KJ_IF_SOME(emitter, emitter_) {
    kj::Maybe<veloz::core::EventLoop&> loop_ref = kj::none;
    KJ_IF_SOME(loop, event_loop_) {
      loop_ref = *loop;
    }

    // Create market data manager configuration
    MarketDataManager::Config mdConfig;
    mdConfig.use_testnet = config_.use_testnet;
    mdConfig.auto_reconnect = true;

    // Create market data manager
    market_data_manager_ = kj::heap<MarketDataManager>(io, *emitter, mdConfig, loop_ref);

    KJ_IF_SOME(mdm, market_data_manager_) {
      KJ_IF_SOME(l, logger_) {
        l.info("Starting market data manager...");
      }

      // Start the market data manager
      return mdm->start();
    }
  }

  return kj::READY_NOW;
}

} // namespace veloz::engine
