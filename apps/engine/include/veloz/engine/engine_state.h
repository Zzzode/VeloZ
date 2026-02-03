#pragma once

#include "veloz/exec/order_api.h"
#include "veloz/oms/order_record.h"
#include "veloz/risk/risk_engine.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace veloz::engine {

struct Balance final {
  std::string asset;
  double free{0.0};
  double locked{0.0};
};

struct PendingOrder final {
  veloz::exec::PlaceOrderRequest request;
  std::int64_t accept_ts_ns{0};
  std::int64_t due_fill_ts_ns{0};
  double reserved_value{0.0};
};

struct OrderDecision final {
  bool accepted{false};
  std::string reason;
  std::string venue_order_id;
  PendingOrder pending;
};

struct CancelDecision final {
  bool found{false};
  std::optional<PendingOrder> cancelled;
  std::string reason;
};

class EngineState final {
public:
  EngineState();

  void init_balances();
  [[nodiscard]] std::vector<Balance> snapshot_balances() const;
  [[nodiscard]] double price() const;
  void set_price(double value);

  [[nodiscard]] OrderDecision place_order(const veloz::exec::PlaceOrderRequest& request,
                                          std::int64_t ts_ns);
  [[nodiscard]] CancelDecision cancel_order(std::string_view client_order_id,
                                            std::int64_t ts_ns);
  void apply_fill(const PendingOrder& po, double fill_price, std::int64_t ts_ns);
  [[nodiscard]] std::vector<PendingOrder> collect_due_fills(std::int64_t now_ns);
  [[nodiscard]] std::optional<veloz::oms::OrderState> get_order_state(
      std::string_view client_order_id) const;

private:
  veloz::risk::RiskEngine risk_engine_;
  veloz::oms::OrderStore order_store_;
  std::atomic<double> price_{42000.0};
  mutable std::mutex account_mu_;
  std::unordered_map<std::string, Balance> balances_;
  mutable std::mutex orders_mu_;
  std::unordered_map<std::string, PendingOrder> pending_;
  std::uint64_t venue_counter_{0};

  bool has_duplicate(std::string_view client_order_id) const;
  bool reserve_for_order(const veloz::exec::PlaceOrderRequest& request, std::int64_t ts_ns,
                         std::string& reason);
  void release_on_cancel(const PendingOrder& po);
};

}
