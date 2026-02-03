#include "veloz/engine/stdio_engine.h"

#include "veloz/core/time.h"
#include "veloz/engine/command_parser.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace veloz::engine {

namespace {

double clamp(double v, double lo, double hi) {
  return (v < lo) ? lo : ((v > hi) ? hi : v);
}

std::string side_to_string(veloz::exec::OrderSide side) {
  return (side == veloz::exec::OrderSide::Sell) ? "SELL" : "BUY";
}

}

StdioEngine::StdioEngine(std::ostream& out) : emitter_(out) {}

int StdioEngine::run(std::atomic<bool>& stop_flag) {
  state_.init_balances();
  emitter_.emit_account(veloz::core::now_unix_ns(), state_.snapshot_balances());

  std::jthread market_thread([&] {
    using namespace std::chrono_literals;
    std::uint64_t tick = 0;
    while (!stop_flag.load()) {
      tick++;
      const double drift = (static_cast<double>((tick % 19)) - 9.0) * 0.25;
      const double noise = (static_cast<double>((tick % 7)) - 3.0) * 0.10;
      double next = state_.price() + drift + noise;
      next = clamp(next, 1000.0, 1'000'000.0);
      state_.set_price(next);
      const auto ts = veloz::core::now_unix_ns();
      emitter_.emit_market("BTCUSDT", next, ts);
      std::this_thread::sleep_for(200ms);
    }
  });

  std::jthread fill_thread([&] {
    using namespace std::chrono_literals;
    while (!stop_flag.load()) {
      const auto now = veloz::core::now_unix_ns();
      auto due = state_.collect_due_fills(now);
      for (const auto& po : due) {
        const auto ts = veloz::core::now_unix_ns();
        const double fill_price = state_.price();
        emitter_.emit_fill(po.request.client_order_id, po.request.symbol.value, po.request.qty,
                           fill_price, ts);
        state_.apply_fill(po, fill_price, ts);
        if (const auto st = state_.get_order_state(po.request.client_order_id); st.has_value()) {
          emitter_.emit_order_state(st.value());
        }
        emitter_.emit_account(ts, state_.snapshot_balances());
      }
      std::this_thread::sleep_for(20ms);
    }
  });

  std::string line;
  while (!stop_flag.load() && std::getline(std::cin, line)) {
    const auto parsed = parse_order_command(line);
    if (parsed.has_value()) {
      const auto ts = veloz::core::now_unix_ns();
      const auto decision = state_.place_order(parsed->request, ts);
      if (!decision.accepted) {
        emitter_.emit_order_update(parsed->request.client_order_id, "REJECTED",
                                   parsed->request.symbol.value,
                                   side_to_string(parsed->request.side), parsed->request.qty,
                                   parsed->request.price.value_or(0.0), "", decision.reason, ts);
        if (const auto st = state_.get_order_state(parsed->request.client_order_id);
            st.has_value()) {
          emitter_.emit_order_state(st.value());
        }
        continue;
      }
      emitter_.emit_order_update(parsed->request.client_order_id, "ACCEPTED",
                                 parsed->request.symbol.value,
                                 side_to_string(parsed->request.side), parsed->request.qty,
                                 parsed->request.price.value_or(0.0), decision.venue_order_id, "",
                                 ts);
      if (const auto st = state_.get_order_state(parsed->request.client_order_id);
          st.has_value()) {
        emitter_.emit_order_state(st.value());
      }
      emitter_.emit_account(ts, state_.snapshot_balances());
      continue;
    }

    const auto cancel = parse_cancel_command(line);
    if (cancel.has_value()) {
      const auto ts = veloz::core::now_unix_ns();
      const auto decision = state_.cancel_order(cancel->client_order_id, ts);
      if (decision.found && decision.cancelled.has_value()) {
        const auto& po = decision.cancelled.value();
        emitter_.emit_order_update(po.request.client_order_id, "CANCELLED",
                                   po.request.symbol.value, side_to_string(po.request.side),
                                   po.request.qty, po.request.price.value_or(0.0), "", "", ts);
        if (const auto st = state_.get_order_state(po.request.client_order_id); st.has_value()) {
          emitter_.emit_order_state(st.value());
        }
        emitter_.emit_account(ts, state_.snapshot_balances());
      } else {
        emitter_.emit_order_update(cancel->client_order_id, "REJECTED", "", "", std::nullopt,
                                   std::nullopt, "", decision.reason, ts);
        if (const auto st = state_.get_order_state(cancel->client_order_id); st.has_value()) {
          emitter_.emit_order_state(st.value());
        }
      }
      continue;
    }

    emitter_.emit_error("bad_command", veloz::core::now_unix_ns());
  }

  stop_flag.store(true);
  return 0;
}

}
