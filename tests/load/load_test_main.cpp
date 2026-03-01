/**
 * @file load_test_main.cpp
 * @brief Main entry point for VeloZ load tests
 *
 * This file runs comprehensive load tests including:
 * 1. Market data throughput test (100k+ events/sec)
 * 2. Order placement throughput test
 * 3. Sustained load test for memory leak detection
 */

#include "load_test_framework.h"

#include "veloz/core/event_loop.h"
#include "veloz/core/json.h"
#include "veloz/market/order_book.h"
#include "veloz/oms/position.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <kj/debug.h>
#include <kj/main.h>

using namespace veloz::loadtest;

namespace {

// ============================================================================
// Mock Components for Load Testing
// ============================================================================

/**
 * @brief Mock market data processor that simulates realistic processing
 */
class MockMarketDataProcessor {
public:
  void process(const veloz::market::MarketEvent& event) {
    // Simulate order book update
    if (event.type == veloz::market::MarketEventType::BookTop ||
        event.type == veloz::market::MarketEventType::BookDelta) {
      // Simulate processing time
      volatile double sum = 0;
      for (int i = 0; i < 10; ++i) {
        sum += static_cast<double>(i) * 0.1;
      }
      (void)sum;
    }

    // Simulate trade processing
    if (event.type == veloz::market::MarketEventType::Trade) {
      // Simulate processing time
      volatile double sum = 0;
      for (int i = 0; i < 5; ++i) {
        sum += static_cast<double>(i) * 0.1;
      }
      (void)sum;
    }

    events_processed_.fetch_add(1, std::memory_order_relaxed);
  }

  [[nodiscard]] uint64_t events_processed() const {
    return events_processed_.load(std::memory_order_relaxed);
  }

private:
  std::atomic<uint64_t> events_processed_{0};
};

/**
 * @brief Mock order processor that simulates realistic order handling
 */
class MockOrderProcessor {
public:
  void process(const veloz::exec::PlaceOrderRequest& request) {
    // Simulate order validation
    if (request.qty <= 0) {
      throw std::runtime_error("Invalid quantity");
    }

    // Simulate risk check
    volatile double risk_score = 0;
    for (int i = 0; i < 5; ++i) {
      risk_score += static_cast<double>(i) * 0.1;
    }
    (void)risk_score;

    // Simulate order book lookup
    volatile double price_check = 0;
    KJ_IF_SOME(price, request.price) {
      price_check = price * 1.001;
    }
    (void)price_check;

    orders_processed_.fetch_add(1, std::memory_order_relaxed);
  }

  [[nodiscard]] uint64_t orders_processed() const {
    return orders_processed_.load(std::memory_order_relaxed);
  }

private:
  std::atomic<uint64_t> orders_processed_{0};
};

// ============================================================================
// Load Test Scenarios
// ============================================================================

void run_quick_test() {
  KJ_LOG(INFO, "Running quick load test (30 seconds)...");

  LoadTestRunner::Config config;
  config.duration_sec = 30;
  config.target_events_per_sec = 50000.0;
  config.target_orders_per_sec = 1000.0;
  config.market_config.num_symbols = 500;
  config.order_config.num_symbols = 100;
  config.report_interval_sec = 10;

  LoadTestRunner runner(config);
  LoadTestSuite suite("Quick Load Test");

  MockMarketDataProcessor market_processor;
  MockOrderProcessor order_processor;

  // Market data test
  auto market_result = runner.run_market_data_test(
      [&market_processor](const veloz::market::MarketEvent& event) {
        market_processor.process(event);
      });
  suite.add_result(kj::mv(market_result));

  // Order test
  auto order_result = runner.run_order_test(
      [&order_processor](const veloz::exec::PlaceOrderRequest& request) {
        order_processor.process(request);
      });
  suite.add_result(kj::mv(order_result));

  // Print report
  kj::String report = suite.generate_report();
  std::printf("%s\n", report.cStr());

  if (!suite.all_passed()) {
    KJ_LOG(WARNING, "Some tests failed!");
  }
}

void run_full_test() {
  KJ_LOG(INFO, "Running full load test (5 minutes per test)...");

  LoadTestRunner::Config config;
  config.duration_sec = 300; // 5 minutes
  config.target_events_per_sec = 100000.0;
  config.target_orders_per_sec = 5000.0;
  config.market_config.num_symbols = 1000;
  config.order_config.num_symbols = 200;
  config.report_interval_sec = 30;

  LoadTestRunner runner(config);
  LoadTestSuite suite("Full Load Test");

  MockMarketDataProcessor market_processor;
  MockOrderProcessor order_processor;

  // Market data test
  KJ_LOG(INFO, "Starting market data throughput test...");
  auto market_result = runner.run_market_data_test(
      [&market_processor](const veloz::market::MarketEvent& event) {
        market_processor.process(event);
      });
  suite.add_result(kj::mv(market_result));

  // Order test
  KJ_LOG(INFO, "Starting order throughput test...");
  auto order_result = runner.run_order_test(
      [&order_processor](const veloz::exec::PlaceOrderRequest& request) {
        order_processor.process(request);
      });
  suite.add_result(kj::mv(order_result));

  // Print report
  kj::String report = suite.generate_report();
  std::printf("%s\n", report.cStr());

  // Save JSON report
  kj::String json_report = suite.to_json();
  std::printf("\nJSON Report:\n%s\n", json_report.cStr());

  if (!suite.all_passed()) {
    KJ_LOG(WARNING, "Some tests failed!");
  }
}

void run_sustained_test(uint64_t hours) {
  KJ_LOG(INFO, "Running sustained load test (", hours, " hours)...");

  LoadTestRunner::Config config;
  config.duration_sec = 60; // Not used for sustained test
  config.target_events_per_sec = 50000.0; // Reduced for sustained
  config.target_orders_per_sec = 1000.0;
  config.market_config.num_symbols = 500;
  config.order_config.num_symbols = 100;

  LoadTestRunner runner(config);
  LoadTestSuite suite("Sustained Load Test");

  MockMarketDataProcessor market_processor;
  MockOrderProcessor order_processor;

  auto result = runner.run_sustained_test(
      [&market_processor](const veloz::market::MarketEvent& event) {
        market_processor.process(event);
      },
      [&order_processor](const veloz::exec::PlaceOrderRequest& request) {
        order_processor.process(request);
      },
      hours);
  suite.add_result(kj::mv(result));

  // Print report
  kj::String report = suite.generate_report();
  std::printf("%s\n", report.cStr());

  if (!suite.all_passed()) {
    KJ_LOG(WARNING, "Memory leak or performance degradation detected!");
  }
}

void run_stress_test() {
  KJ_LOG(INFO, "Running stress test (maximum throughput)...");

  LoadTestRunner::Config config;
  config.duration_sec = 60;
  config.target_events_per_sec = 500000.0; // Push to limits
  config.target_orders_per_sec = 50000.0;
  config.market_config.num_symbols = 2000;
  config.order_config.num_symbols = 500;
  config.report_interval_sec = 10;

  // Relax targets for stress test
  config.targets.market_data_p99_us = 100000.0; // 100ms
  config.targets.order_path_p99_us = 50000.0;   // 50ms

  LoadTestRunner runner(config);
  LoadTestSuite suite("Stress Test");

  MockMarketDataProcessor market_processor;
  MockOrderProcessor order_processor;

  // Market data stress test
  auto market_result = runner.run_market_data_test(
      [&market_processor](const veloz::market::MarketEvent& event) {
        market_processor.process(event);
      });
  suite.add_result(kj::mv(market_result));

  // Order stress test
  auto order_result = runner.run_order_test(
      [&order_processor](const veloz::exec::PlaceOrderRequest& request) {
        order_processor.process(request);
      });
  suite.add_result(kj::mv(order_result));

  // Print report
  kj::String report = suite.generate_report();
  std::printf("%s\n", report.cStr());

  KJ_LOG(INFO, "Stress test completed. Maximum achieved throughput recorded.");
}

void print_usage() {
  std::printf(
      "VeloZ Load Testing Framework\n"
      "\n"
      "Usage: veloz_load_tests [command] [options]\n"
      "\n"
      "Commands:\n"
      "  quick      Run quick load test (30 seconds)\n"
      "  full       Run full load test (5 minutes per test)\n"
      "  sustained  Run sustained test for memory leak detection\n"
      "  stress     Run stress test (maximum throughput)\n"
      "  help       Show this help message\n"
      "\n"
      "Options:\n"
      "  --hours N  Duration for sustained test (default: 1)\n"
      "\n"
      "Examples:\n"
      "  veloz_load_tests quick\n"
      "  veloz_load_tests full\n"
      "  veloz_load_tests sustained --hours 24\n"
      "  veloz_load_tests stress\n"
  );
}

} // namespace

int main(int argc, char* argv[]) {
  try {
    if (argc < 2) {
      print_usage();
      return 1;
    }

    kj::StringPtr command = argv[1];

    if (command == "quick") {
      run_quick_test();
    } else if (command == "full") {
      run_full_test();
    } else if (command == "sustained") {
      uint64_t hours = 1;
      for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--hours") == 0 && i + 1 < argc) {
          hours = static_cast<uint64_t>(std::atol(argv[i + 1]));
          ++i;
        }
      }
      run_sustained_test(hours);
    } else if (command == "stress") {
      run_stress_test();
    } else if (command == "help" || command == "--help" || command == "-h") {
      print_usage();
    } else {
      std::fprintf(stderr, "Unknown command: %s\n", command.cStr());
      print_usage();
      return 1;
    }

    return 0;
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Load test failed with exception", e.getDescription());
    return 1;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "Load test failed: %s\n", e.what());
    return 1;
  }
}
