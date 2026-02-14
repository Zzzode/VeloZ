#pragma once

#include "veloz/exec/order_api.h"
#include "veloz/oms/order_record.h"
#include "veloz/risk/risk_engine.h"

#include <atomic>
#include <cstdint>
#include <kj/common.h> // kj::Maybe is defined here
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <mutex>
#include <unordered_map>

namespace veloz::engine {

struct Balance final {
  kj::String asset;
  double free{0.0};
  double locked{0.0};

  Balance() : asset(kj::str("")) {}
};

struct PendingOrder final {
  veloz::exec::PlaceOrderRequest request;
  std::int64_t accept_ts_ns{0};
  std::int64_t due_fill_ts_ns{0};
  double reserved_value{0.0};
};

struct OrderDecision final {
  bool accepted{false};
  kj::String reason;
  kj::String venue_order_id;
  PendingOrder pending;

  OrderDecision() : reason(kj::str("")), venue_order_id(kj::str("")) {}
};

struct CancelDecision final {
  bool found{false};
  kj::Maybe<PendingOrder> cancelled;
  kj::String reason;

  CancelDecision() : reason(kj::str("")) {}
};

class EngineState final {
public:
  EngineState();

  void init_balances();
  [[nodiscard]] kj::Vector<Balance> snapshot_balances() const;
  [[nodiscard]] double price() const;
  void set_price(double value);

  [[nodiscard]] OrderDecision place_order(const veloz::exec::PlaceOrderRequest& request,
                                          std::int64_t ts_ns);
  [[nodiscard]] CancelDecision cancel_order(kj::StringPtr client_order_id, std::int64_t ts_ns);
  void apply_fill(const PendingOrder& po, double fill_price, std::int64_t ts_ns);
  [[nodiscard]] kj::Vector<PendingOrder> collect_due_fills(std::int64_t now_ns);
  [[nodiscard]] kj::Maybe<veloz::oms::OrderState>
  get_order_state(kj::StringPtr client_order_id) const;

private:
  veloz::risk::RiskEngine risk_engine_;
  veloz::oms::OrderStore order_store_;
  std::atomic<double> price_{42000.0};
  // std::mutex used for thread synchronization with std::unordered_map
  mutable std::mutex account_mu_;
  // std::unordered_map used for O(1) lookup by string key
  std::unordered_map<std::string, Balance> balances_;
  mutable std::mutex orders_mu_;
  std::unordered_map<std::string, PendingOrder> pending_;
  std::uint64_t venue_counter_{0};

  bool has_duplicate(kj::StringPtr client_order_id) const;
  bool reserve_for_order(const veloz::exec::PlaceOrderRequest& request, std::int64_t ts_ns,
                         kj::String& reason);
  void release_on_cancel(const PendingOrder& po);
};

} // namespace veloz::engine
