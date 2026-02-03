#include "veloz/engine/engine_app.h"

#include "veloz/core/event_loop.h"
#include "veloz/core/logger.h"
#include "veloz/engine/stdio_engine.h"

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

} 

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
  veloz::core::Logger logger(config_.stdio_mode ? err_ : out_);
  logger.set_level(veloz::core::LogLevel::Info);
  logger_ = &logger;
  logger.info(config_.stdio_mode ? "VeloZ engine starting (stdio)" : "VeloZ engine starting");

  if (config_.stdio_mode) {
    return run_stdio();
  }
  return run_service();
}

int EngineApp::run_stdio() {
  StdioEngine engine(out_);
  const int rc = engine.run(stop_);
  logger_->info("shutdown requested");
  return rc;
}

int EngineApp::run_service() {
  veloz::core::EventLoop loop;
  std::jthread loop_thread([&] { loop.run(); });
  std::jthread heartbeat([&] {
    using namespace std::chrono_literals;
    while (!stop_.load()) {
      loop.post([&] { logger_->info("heartbeat"); });
      std::this_thread::sleep_for(1s);
    }
  });

  while (!stop_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  logger_->info("shutdown requested");
  loop.stop();
  loop_thread.join();
  logger_->info("VeloZ engine stopped");
  return 0;
}

}
