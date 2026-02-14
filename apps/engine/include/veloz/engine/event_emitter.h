#pragma once

#include "veloz/engine/engine_state.h"
#include "veloz/oms/order_record.h"

#include <cstdint>
#include <kj/common.h> // kj::Maybe is defined here
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <ostream>

namespace veloz::engine {

class EventEmitter final {
public:
  explicit EventEmitter(std::ostream& out);

  void emit_market(kj::StringPtr symbol, double price, std::int64_t ts_ns);
  void emit_fill(kj::StringPtr client_order_id, kj::StringPtr symbol, double qty, double price,
                 std::int64_t ts_ns);
  void emit_order_update(kj::StringPtr client_order_id, kj::StringPtr status, kj::StringPtr symbol,
                         kj::StringPtr side, kj::Maybe<double> qty, kj::Maybe<double> price,
                         kj::StringPtr venue_order_id, kj::StringPtr reason, std::int64_t ts_ns);
  void emit_order_state(const veloz::oms::OrderState& state);
  void emit_account(std::int64_t ts_ns, const kj::Vector<Balance>& balances);
  void emit_error(kj::StringPtr message, std::int64_t ts_ns);

private:
  // std::ostream used for external API compatibility with C++ standard output
  std::ostream& out_;
  kj::MutexGuarded<int> mu_;

  void emit_line(kj::StringPtr json_line);
};

} // namespace veloz::engine
