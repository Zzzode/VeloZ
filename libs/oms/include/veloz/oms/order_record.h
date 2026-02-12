#pragma once

#include "veloz/exec/order_api.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace veloz::oms {

class OrderRecord final {
public:
  explicit OrderRecord(veloz::exec::PlaceOrderRequest request);

  [[nodiscard]] const veloz::exec::PlaceOrderRequest& request() const;
  [[nodiscard]] const std::string& venue_order_id() const;
  [[nodiscard]] veloz::exec::OrderStatus status() const;
  [[nodiscard]] double cum_qty() const;
  [[nodiscard]] double avg_price() const;
  [[nodiscard]] std::int64_t last_update_ts() const;
  [[nodiscard]] bool is_terminal() const;

  void apply(const veloz::exec::ExecutionReport& report);

private:
  veloz::exec::PlaceOrderRequest request_;
  std::string venue_order_id_;
  veloz::exec::OrderStatus status_{veloz::exec::OrderStatus::New};
  double cum_qty_{0.0};
  double avg_price_{0.0};
  std::int64_t last_update_ts_{0};
};

struct OrderState final {
  std::string client_order_id;
  std::string symbol;
  std::string side;
  std::optional<double> order_qty;
  std::optional<double> limit_price;
  double executed_qty{0.0};
  double avg_price{0.0};
  std::string venue_order_id;
  std::string status;
  std::string reason;
  std::int64_t last_ts_ns{0};
  std::int64_t created_ts_ns{0};
};

class OrderStore final {
public:
  OrderStore() = default;

  void note_order_params(const veloz::exec::PlaceOrderRequest& request);
  void apply_order_update(std::string_view client_order_id, std::string_view symbol,
                          std::string_view side, std::string_view venue_order_id,
                          std::string_view status, std::string_view reason, std::int64_t ts_ns);
  void apply_fill(std::string_view client_order_id, std::string_view symbol, double qty,
                  double price, std::int64_t ts_ns);
  void apply_execution_report(const veloz::exec::ExecutionReport& report);

  [[nodiscard]] std::optional<OrderState> get(std::string_view client_order_id) const;
  [[nodiscard]] std::vector<OrderState> list() const;
  [[nodiscard]] std::vector<OrderState> list_pending() const;
  [[nodiscard]] std::vector<OrderState> list_terminal() const;

  [[nodiscard]] size_t count() const;
  [[nodiscard]] size_t count_pending() const;
  [[nodiscard]] size_t count_terminal() const;

  void clear();

private:
  mutable std::mutex mu_;
  std::unordered_map<std::string, OrderState> orders_;
};

} // namespace veloz::oms
