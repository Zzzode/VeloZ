#include "veloz/core/event_loop.h"
#include "veloz/core/logger.h"
#include "veloz/core/time.h"
#include "veloz/exec/order_api.h"
#include "veloz/market/market_event.h"
#include "veloz/oms/order_record.h"
#include "veloz/risk/risk_engine.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

std::atomic<bool> g_stop{false};

void handle_signal(int) {
  g_stop.store(true);
}

[[nodiscard]] bool has_flag(const std::vector<std::string>& args, std::string_view flag) {
  for (const auto& a : args) {
    if (a == flag) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::string escape_json(std::string_view s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  return out;
}

struct ParsedOrder final {
  veloz::exec::PlaceOrderRequest req;
};

struct ParsedCancel final {
  std::string client_order_id;
};

[[nodiscard]] std::optional<ParsedOrder> parse_order_command(std::string_view line) {
  std::istringstream iss{std::string(line)};
  std::string verb;
  iss >> verb;
  if (verb != "ORDER") {
    return std::nullopt;
  }

  std::string side_s;
  std::string symbol;
  double qty = 0.0;
  double price = 0.0;
  std::string client_id;

  iss >> side_s >> symbol >> qty >> price >> client_id;
  if (side_s.empty() || symbol.empty() || qty <= 0.0 || price <= 0.0 || client_id.empty()) {
    return std::nullopt;
  }

  ParsedOrder out;
  out.req.symbol.value = symbol;
  out.req.side = (side_s == "SELL" || side_s == "Sell" || side_s == "sell")
                     ? veloz::exec::OrderSide::Sell
                     : veloz::exec::OrderSide::Buy;
  out.req.type = veloz::exec::OrderType::Limit;
  out.req.tif = veloz::exec::TimeInForce::GTC;
  out.req.qty = qty;
  out.req.price = price;
  out.req.client_order_id = client_id;
  return out;
}

[[nodiscard]] std::optional<ParsedCancel> parse_cancel_command(std::string_view line) {
  std::istringstream iss{std::string(line)};
  std::string verb;
  iss >> verb;
  if (verb != "CANCEL") {
    return std::nullopt;
  }
  std::string client_id;
  iss >> client_id;
  if (client_id.empty()) {
    return std::nullopt;
  }
  return ParsedCancel{.client_order_id = client_id};
}

[[nodiscard]] double clamp(double v, double lo, double hi) {
  return (v < lo) ? lo : ((v > hi) ? hi : v);
}

} // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    args.emplace_back(argv[i] ? argv[i] : "");
  }

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  const bool stdio_mode = has_flag(args, "--stdio");

  veloz::core::Logger logger(stdio_mode ? std::cerr : std::cout);
  logger.set_level(veloz::core::LogLevel::Info);

  logger.info(stdio_mode ? "VeloZ engine starting (stdio)" : "VeloZ engine starting");

  if (stdio_mode) {
    veloz::risk::RiskEngine risk_engine;
    std::atomic<double> price{42000.0};
    std::mutex out_mu;

    auto emit = [&](std::string_view json_line) {
      std::lock_guard<std::mutex> lk(out_mu);
      std::cout << json_line << '\n' << std::flush;
    };

    struct PendingOrder final {
      veloz::exec::PlaceOrderRequest req;
      std::int64_t accept_ts_ns{0};
      std::int64_t due_fill_ts_ns{0};
    };

    std::mutex orders_mu;
    std::unordered_map<std::string, PendingOrder> pending;
    std::uint64_t venue_counter = 0;

    std::jthread market_thread([&] {
      using namespace std::chrono_literals;
      std::uint64_t tick = 0;
      while (!g_stop.load()) {
        tick++;
        const double drift = (static_cast<double>((tick % 19)) - 9.0) * 0.25;
        const double noise = (static_cast<double>((tick % 7)) - 3.0) * 0.10;
        double next = price.load() + drift + noise;
        next = clamp(next, 1000.0, 1'000'000.0);
        price.store(next);

        const auto ts = veloz::core::now_unix_ns();
        std::ostringstream oss;
        oss << "{\"type\":\"market\",\"symbol\":\"BTCUSDT\",\"ts_ns\":" << ts
            << ",\"price\":" << next << "}";
        emit(oss.str());
        std::this_thread::sleep_for(200ms);
      }
    });

    std::jthread fill_thread([&] {
      using namespace std::chrono_literals;
      while (!g_stop.load()) {
        std::vector<PendingOrder> to_fill;
        const auto now = veloz::core::now_unix_ns();
        {
          std::lock_guard<std::mutex> lk(orders_mu);
          for (auto it = pending.begin(); it != pending.end();) {
            if (it->second.due_fill_ts_ns <= now) {
              to_fill.push_back(it->second);
              it = pending.erase(it);
            } else {
              ++it;
            }
          }
        }

        for (const auto& po : to_fill) {
          const auto ts = veloz::core::now_unix_ns();
          const double fill_price = price.load();
          std::ostringstream oss_fill;
          oss_fill << "{\"type\":\"fill\",\"ts_ns\":" << ts << ",\"client_order_id\":\""
                   << escape_json(po.req.client_order_id) << "\",\"symbol\":\""
                   << escape_json(po.req.symbol.value) << "\",\"qty\":" << po.req.qty
                   << ",\"price\":" << fill_price << "}";
          emit(oss_fill.str());
        }

        std::this_thread::sleep_for(20ms);
      }
    });

    std::string line;
    while (!g_stop.load() && std::getline(std::cin, line)) {
      const auto parsed = parse_order_command(line);
      if (parsed.has_value()) {
        const auto risk = risk_engine.check(parsed->req);
        const auto ts = veloz::core::now_unix_ns();
        if (!risk.ok) {
          std::ostringstream oss;
          oss << "{\"type\":\"order_update\",\"ts_ns\":" << ts << ",\"client_order_id\":\""
              << escape_json(parsed->req.client_order_id)
              << "\",\"status\":\"REJECTED\",\"reason\":\"" << escape_json(risk.reason) << "\"}";
          emit(oss.str());
          continue;
        }

        {
          std::lock_guard<std::mutex> lk(orders_mu);
          if (pending.contains(parsed->req.client_order_id)) {
            std::ostringstream oss;
            oss << "{\"type\":\"order_update\",\"ts_ns\":" << ts << ",\"client_order_id\":\""
                << escape_json(parsed->req.client_order_id)
                << "\",\"status\":\"REJECTED\",\"reason\":\""
                << "duplicate_client_order_id\"}";
            emit(oss.str());
            continue;
          }
        }

        venue_counter++;
        std::string venue_order_id = "sim-" + std::to_string(venue_counter);

        std::ostringstream oss_acc;
        oss_acc << "{\"type\":\"order_update\",\"ts_ns\":" << ts << ",\"client_order_id\":\""
                << escape_json(parsed->req.client_order_id) << "\",\"venue_order_id\":\""
                << escape_json(venue_order_id) << "\",\"status\":\"ACCEPTED\",\"symbol\":\""
                << escape_json(parsed->req.symbol.value) << "\"}";
        emit(oss_acc.str());

        PendingOrder po;
        po.req = parsed->req;
        po.accept_ts_ns = ts;
        po.due_fill_ts_ns = ts + 300'000'000;
        {
          std::lock_guard<std::mutex> lk(orders_mu);
          pending.emplace(parsed->req.client_order_id, std::move(po));
        }
        continue;
      }

      const auto cancel = parse_cancel_command(line);
      if (cancel.has_value()) {
        const auto ts = veloz::core::now_unix_ns();
        bool removed = false;
        {
          std::lock_guard<std::mutex> lk(orders_mu);
          removed = pending.erase(cancel->client_order_id) > 0;
        }
        if (removed) {
          std::ostringstream oss;
          oss << "{\"type\":\"order_update\",\"ts_ns\":" << ts << ",\"client_order_id\":\""
              << escape_json(cancel->client_order_id) << "\",\"status\":\"CANCELLED\"}";
          emit(oss.str());
        } else {
          std::ostringstream oss;
          oss << "{\"type\":\"order_update\",\"ts_ns\":" << ts << ",\"client_order_id\":\""
              << escape_json(cancel->client_order_id)
              << "\",\"status\":\"REJECTED\",\"reason\":\"unknown_order\"}";
          emit(oss.str());
        }
        continue;
      }

      const auto ts = veloz::core::now_unix_ns();
      emit(std::string("{\"type\":\"error\",\"ts_ns\":") + std::to_string(ts) +
           ",\"message\":\"bad_command\"}");
    }

    g_stop.store(true);
    logger.info("shutdown requested");
    return 0;
  }

  veloz::core::EventLoop loop;
  std::jthread loop_thread([&] { loop.run(); });
  std::jthread heartbeat([&] {
    using namespace std::chrono_literals;
    while (!g_stop.load()) {
      loop.post([&] { logger.info("heartbeat"); });
      std::this_thread::sleep_for(1s);
    }
  });

  while (!g_stop.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  logger.info("shutdown requested");
  loop.stop();
  loop_thread.join();
  logger.info("VeloZ engine stopped");
  return 0;
}
