#include "veloz/engine/engine_app.h"

#include "veloz/core/event_loop.h"
#include "veloz/core/logger.h"
#include "veloz/engine/stdio_engine.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <thread>

namespace {

std::atomic<bool>* g_stop_ptr = nullptr;

void handle_signal(int) {
  if (g_stop_ptr) {
    g_stop_ptr->store(true);
  }
}

} // namespace

namespace veloz::engine {

EngineApp::EngineApp(EngineConfig config, std::ostream& out, std::ostream& err)
    : config_(config), out_(out), err_(err) {}

void EngineApp::install_signal_handlers() {
  g_stop_ptr = &stop_;
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
}

int EngineApp::run() {
  install_signal_handlers();

  // Create logger with appropriate output
  std::unique_ptr<veloz::core::Logger> logger_ptr;
  if (config_.stdio_mode) {
    // Logger for stderr
    auto console_output = std::make_unique<veloz::core::ConsoleOutput>(true);
    logger_ptr = std::make_unique<veloz::core::Logger>(
        std::make_unique<veloz::core::TextFormatter>(), std::move(console_output));
  } else {
    // Logger for stdout
    auto console_output = std::make_unique<veloz::core::ConsoleOutput>(false);
    logger_ptr = std::make_unique<veloz::core::Logger>(
        std::make_unique<veloz::core::TextFormatter>(), std::move(console_output));
  }

  logger_ptr->set_level(veloz::core::LogLevel::Info);
  logger_ptr->info(config_.stdio_mode ? "VeloZ engine starting (stdio)" : "VeloZ engine starting");

  if (config_.stdio_mode) {
    return run_stdio();
  }
  return run_service();
}

int EngineApp::run_stdio() {
  StdioEngine engine(out_);
  const int rc = engine.run(stop_);
  return rc;
}

int EngineApp::run_service() {
  veloz::core::EventLoop loop;
  std::thread loop_thread([&] { loop.run(); });
  std::thread heartbeat([&] {
    using namespace std::chrono_literals;
    while (!stop_.load()) {
      loop.post([&] { /* heartbeat */ });
      std::this_thread::sleep_for(1s);
    }
  });

  while (!stop_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  loop.stop();
  loop_thread.join();
  return 0;
}

} // namespace veloz::engine
