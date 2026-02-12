#pragma once

#include "veloz/engine/engine_state.h"
#include "veloz/oms/order_record.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace veloz::engine {

class EventEmitter final {
public:
  explicit EventEmitter(std::ostream& out);

  void emit_market(std::string_view symbol, double price, std::int64_t ts_ns);
  void emit_fill(std::string_view client_order_id, std::string_view symbol, double qty,
                 double price, std::int64_t ts_ns);
  void emit_order_update(std::string_view client_order_id, std::string_view status,
                         std::string_view symbol, std::string_view side, std::optional<double> qty,
                         std::optional<double> price, std::string_view venue_order_id,
                         std::string_view reason, std::int64_t ts_ns);
  void emit_order_state(const veloz::oms::OrderState& state);
  void emit_account(std::int64_t ts_ns, const std::vector<Balance>& balances);
  void emit_error(std::string_view message, std::int64_t ts_ns);

private:
  std::ostream& out_;
  std::mutex mu_;

  void emit_line(std::string_view json_line);
};

} // namespace veloz::engine
