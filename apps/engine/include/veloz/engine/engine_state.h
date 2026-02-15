#pragma once

#include "veloz/exec/order_api.h"
#include "veloz/oms/order_record.h"
#include "veloz/risk/risk_engine.h"

// std::int64_t, std::uint64_t for fixed-width integers (standard C types, KJ uses these)
#include <cstdint>
#include <kj/common.h>
#include <kj/hash.h>
#include <kj/map.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>

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
  kj::MutexGuarded<double> price_{42000.0};
  kj::MutexGuarded<kj::HashMap<kj::String, Balance>> balances_;
  kj::MutexGuarded<kj::HashMap<kj::String, PendingOrder>> pending_;
  kj::MutexGuarded<std::uint64_t> venue_counter_{0};

  bool has_duplicate(kj::StringPtr client_order_id) const;
  bool reserve_for_order(const veloz::exec::PlaceOrderRequest& request, std::int64_t ts_ns,
                         kj::String& reason);
  void release_on_cancel(const PendingOrder& po);
};

} // namespace veloz::engine
