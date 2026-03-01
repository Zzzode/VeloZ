#include "veloz/exec/binance_reconciliation_adapter.h"

#include <kj/debug.h>

namespace veloz::exec {

BinanceReconciliationAdapter::BinanceReconciliationAdapter(BinanceAdapter& adapter)
    : adapter_(adapter) {}

kj::Promise<kj::Vector<ExecutionReport>>
BinanceReconciliationAdapter::query_open_orders_async(const veloz::common::SymbolId& symbol) {
  return adapter_.get_open_orders_async(symbol).then(
      [](kj::Maybe<kj::Array<ExecutionReport>> result) -> kj::Vector<ExecutionReport> {
        kj::Vector<ExecutionReport> orders;
        KJ_IF_SOME(arr, result) {
          for (auto& report : arr) {
            orders.add(kj::mv(report));
          }
        }
        return orders;
      });
}

kj::Promise<kj::Maybe<ExecutionReport>>
BinanceReconciliationAdapter::query_order_async(const veloz::common::SymbolId& symbol,
                                                kj::StringPtr client_order_id) {
  return adapter_.get_order_async(symbol, client_order_id);
}

kj::Promise<kj::Vector<ExecutionReport>> BinanceReconciliationAdapter::query_orders_async(
    const veloz::common::SymbolId& symbol, std::int64_t start_time_ms, std::int64_t end_time_ms) {
  // Binance doesn't have a direct time-range query for orders in the spot API
  // We query open orders and filter by time on the client side
  // For historical orders, you would need to use GET /api/v3/allOrders endpoint
  (void)start_time_ms;
  (void)end_time_ms;

  return adapter_.get_open_orders_async(symbol).then(
      [start_time_ms,
       end_time_ms](kj::Maybe<kj::Array<ExecutionReport>> result) -> kj::Vector<ExecutionReport> {
        kj::Vector<ExecutionReport> orders;
        KJ_IF_SOME(arr, result) {
          for (auto& report : arr) {
            // Filter by time range (exchange timestamp is in nanoseconds)
            auto ts_ms = report.ts_exchange_ns / 1000000;
            if (ts_ms >= start_time_ms && ts_ms <= end_time_ms) {
              orders.add(kj::mv(report));
            }
          }
        }
        return orders;
      });
}

kj::Promise<kj::Maybe<ExecutionReport>>
BinanceReconciliationAdapter::cancel_order_async(const veloz::common::SymbolId& symbol,
                                                 kj::StringPtr client_order_id) {
  CancelOrderRequest req;
  req.symbol = symbol;
  req.client_order_id = kj::heapString(client_order_id);

  return adapter_.cancel_order_async(req);
}

} // namespace veloz::exec
