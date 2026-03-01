#pragma once

#include "veloz/engine/command_parser.h"
#include "veloz/market/market_event.h"
#include "veloz/oms/position.h"
#include "veloz/strategy/strategy.h"

// std::int64_t for fixed-width integers (standard C types, KJ uses these)
#include <cstdint>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/io.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>

namespace veloz {
namespace engine {

class StdioEngine final {
public:
  explicit StdioEngine(kj::OutputStream& out, kj::InputStream& in);
  ~StdioEngine() = default;

  int run(kj::MutexGuarded<bool>& stop_flag);

  // Command handlers
  using OrderHandler = kj::Function<void(const ParsedOrder&)>;
  using CancelHandler = kj::Function<void(const ParsedCancel&)>;
  using QueryHandler = kj::Function<void(const ParsedQuery&)>;
  using StrategyHandler = kj::Function<void(const ParsedStrategy&)>;

  void set_order_handler(OrderHandler handler) {
    order_handler_ = kj::mv(handler);
  }
  void set_cancel_handler(CancelHandler handler) {
    cancel_handler_ = kj::mv(handler);
  }
  void set_query_handler(QueryHandler handler) {
    query_handler_ = kj::mv(handler);
  }
  void set_strategy_handler(StrategyHandler handler) {
    strategy_handler_ = kj::mv(handler);
  }

  // Strategy runtime integration
  veloz::strategy::StrategyManager& get_strategy_manager() {
    return *strategy_manager_;
  }

  // Route market events to all running strategies
  void on_market_event(const veloz::market::MarketEvent& event) {
    strategy_manager_->on_market_event(event);
  }

  // Route position updates to all running strategies
  void on_position_update(const veloz::oms::Position& position) {
    strategy_manager_->on_position_update(position);
  }

  // Get all trading signals from strategies
  kj::Vector<veloz::exec::PlaceOrderRequest> get_strategy_signals() {
    return strategy_manager_->get_all_signals();
  }

  // Process signals and route to execution (via callback)
  void process_strategy_signals() {
    strategy_manager_->process_and_route_signals();
  }

private:
  kj::OutputStream& out_;
  kj::InputStream& in_;
  kj::MutexGuarded<int> output_mutex_;
  kj::Maybe<OrderHandler> order_handler_;
  kj::Maybe<CancelHandler> cancel_handler_;
  kj::Maybe<QueryHandler> query_handler_;
  kj::Maybe<StrategyHandler> strategy_handler_;
  std::int64_t command_count_;
  kj::Own<veloz::strategy::StrategyManager> strategy_manager_;

  void emit_event(kj::StringPtr event_json);
  void emit_error(kj::StringPtr error_msg);
  void handle_strategy_command(const ParsedStrategy& cmd);
};

} // namespace engine
} // namespace veloz
