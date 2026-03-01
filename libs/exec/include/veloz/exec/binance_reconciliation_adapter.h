#pragma once

#include "veloz/exec/binance_adapter.h"
#include "veloz/exec/reconciliation.h"

#include <kj/async.h>
#include <kj/common.h>

namespace veloz::exec {

// Adapter wrapper that implements ReconciliationQueryInterface using BinanceAdapter
// This allows the AccountReconciler to query and cancel orders on Binance
class BinanceReconciliationAdapter final : public ReconciliationQueryInterface {
public:
  explicit BinanceReconciliationAdapter(BinanceAdapter& adapter);
  ~BinanceReconciliationAdapter() noexcept override = default;

  // Query open orders from exchange
  kj::Promise<kj::Vector<ExecutionReport>>
  query_open_orders_async(const veloz::common::SymbolId& symbol) override;

  // Query specific order by client order ID
  kj::Promise<kj::Maybe<ExecutionReport>> query_order_async(const veloz::common::SymbolId& symbol,
                                                            kj::StringPtr client_order_id) override;

  // Query all orders within a time window
  kj::Promise<kj::Vector<ExecutionReport>> query_orders_async(const veloz::common::SymbolId& symbol,
                                                              std::int64_t start_time_ms,
                                                              std::int64_t end_time_ms) override;

  // Cancel an order on the exchange (used for orphaned order cleanup)
  kj::Promise<kj::Maybe<ExecutionReport>>
  cancel_order_async(const veloz::common::SymbolId& symbol, kj::StringPtr client_order_id) override;

private:
  BinanceAdapter& adapter_;
};

} // namespace veloz::exec
