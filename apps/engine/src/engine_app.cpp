#include "veloz/engine/engine_app.h"

#include "veloz/core/event_loop.h"
#include "veloz/core/logger.h"
#include "veloz/engine/stdio_engine.h"

// std::signal for POSIX signal handling (standard C library, KJ lacks signal API)
#include <csignal>
#include <kj/async-io.h>
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

  veloz::core::EventLoop loop;

  // Create a promise that runs the event loop
  auto loopPromise = kj::evalLater([&loop]() { loop.run(); });

  // Heartbeat using KJ timer
  kj::Promise<void> heartbeatPromise = kj::READY_NOW;
  auto runHeartbeat = [&]() -> kj::Promise<void> {
    while (!*stop_.lockShared()) {
      loop.post([&] { /* heartbeat */ });
      co_await timer.afterDelay(1 * kj::SECONDS);
    }
  };
  heartbeatPromise = runHeartbeat();

  // Main loop using KJ timer
  while (!*stop_.lockShared()) {
    timer.afterDelay(50 * kj::MILLISECONDS).wait(io.waitScope);
  }

  loop.stop();
  return 0;
}

} // namespace veloz::engine
