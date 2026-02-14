#pragma once

#include "veloz/exec/order_api.h"

#include <chrono>
#include <cstdint>
#include <kj/common.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <unordered_map> // std::unordered_map for hash-based key lookup, KJ has kj::HashMap but different API

namespace veloz::oms {

class OrderRecord final {
public:
  explicit OrderRecord(veloz::exec::PlaceOrderRequest request);

  [[nodiscard]] const veloz::exec::PlaceOrderRequest& request() const;
  [[nodiscard]] kj::StringPtr venue_order_id() const;
  [[nodiscard]] veloz::exec::OrderStatus status() const;
  [[nodiscard]] double cum_qty() const;
  [[nodiscard]] double avg_price() const;
  [[nodiscard]] std::int64_t last_update_ts() const;
  [[nodiscard]] bool is_terminal() const;

  void apply(const veloz::exec::ExecutionReport& report);

private:
  veloz::exec::PlaceOrderRequest request_;
  kj::String venue_order_id_;
  veloz::exec::OrderStatus status_{veloz::exec::OrderStatus::New};
  double cum_qty_{0.0};
  double avg_price_{0.0};
  std::int64_t last_update_ts_{0};
};

struct OrderState final {
  kj::String client_order_id;
  kj::String symbol;
  kj::String side;
  kj::Maybe<double> order_qty;
  kj::Maybe<double> limit_price;
  double executed_qty{0.0};
  double avg_price{0.0};
  kj::String venue_order_id;
  kj::String status;
  kj::String reason;
  std::int64_t last_ts_ns{0};
  std::int64_t created_ts_ns{0};
};

class OrderStore final {
public:
  OrderStore() = default;

  void note_order_params(const veloz::exec::PlaceOrderRequest& request);
  void apply_order_update(kj::StringPtr client_order_id, kj::StringPtr symbol, kj::StringPtr side,
                          kj::StringPtr venue_order_id, kj::StringPtr status, kj::StringPtr reason,
                          std::int64_t ts_ns);
  void apply_fill(kj::StringPtr client_order_id, kj::StringPtr symbol, double qty, double price,
                  std::int64_t ts_ns);
  void apply_execution_report(const veloz::exec::ExecutionReport& report);

  [[nodiscard]] kj::Maybe<OrderState> get(kj::StringPtr client_order_id) const;
  [[nodiscard]] kj::Vector<OrderState> list() const;
  [[nodiscard]] kj::Vector<OrderState> list_pending() const;
  [[nodiscard]] kj::Vector<OrderState> list_terminal() const;
  [[nodiscard]] size_t count() const;
  [[nodiscard]] size_t count_pending() const;
  [[nodiscard]] size_t count_terminal() const;

  void clear();

private:
  // Internal state for OrderStore
  // Using std::unordered_map for hash-based key lookup with std::string keys
  // since kj::String doesn't have a standard hash function for std::unordered_map
  struct StoreState {
    std::unordered_map<std::string, OrderState> orders;
  };

  kj::MutexGuarded<StoreState> guarded_;
};

} // namespace veloz::oms
