#include "veloz/exec/reconciliation.h"

#include <chrono>
#include <cmath>
#include <kj/debug.h>

namespace veloz::exec {

kj::StringPtr to_string(ReconciliationEventType type) {
  switch (type) {
  case ReconciliationEventType::Started:
    return "Started"_kj;
  case ReconciliationEventType::Completed:
    return "Completed"_kj;
  case ReconciliationEventType::StateMismatch:
    return "StateMismatch"_kj;
  case ReconciliationEventType::OrphanedOrderFound:
    return "OrphanedOrderFound"_kj;
  case ReconciliationEventType::OrderCorrected:
    return "OrderCorrected"_kj;
  case ReconciliationEventType::OrderCancelled:
    return "OrderCancelled"_kj;
  case ReconciliationEventType::Error:
    return "Error"_kj;
  case ReconciliationEventType::StrategyFrozen:
    return "StrategyFrozen"_kj;
  case ReconciliationEventType::StrategyResumed:
    return "StrategyResumed"_kj;
  default:
    return "Unknown"_kj;
  }
}

kj::StringPtr to_string(ReconciliationAction action) {
  switch (action) {
  case ReconciliationAction::None:
    return "None"_kj;
  case ReconciliationAction::UpdateLocalState:
    return "UpdateLocalState"_kj;
  case ReconciliationAction::CancelOrphanedOrder:
    return "CancelOrphanedOrder"_kj;
  case ReconciliationAction::FreezeStrategy:
    return "FreezeStrategy"_kj;
  case ReconciliationAction::ManualIntervention:
    return "ManualIntervention"_kj;
  default:
    return "Unknown"_kj;
  }
}

AccountReconciler::AccountReconciler(kj::AsyncIoContext& io_context, oms::OrderStore& order_store,
                                     ReconciliationConfig config)
    : io_context_(io_context), order_store_(order_store), config_(kj::mv(config)),
      stop_promise_(kj::READY_NOW) {
  // Initialize stop promise/fulfiller pair
  auto paf = kj::newPromiseAndFulfiller<void>();
  stop_promise_ = kj::mv(paf.promise);
  stop_fulfiller_ = kj::mv(paf.fulfiller);
}

void AccountReconciler::register_exchange(veloz::common::Venue venue,
                                          ReconciliationQueryInterface& query_interface) {
  auto lock = guarded_.lockExclusive();
  lock->exchanges.upsert(
      venue, &query_interface,
      [](ReconciliationQueryInterface*& existing, ReconciliationQueryInterface*&& replacement) {
        existing = replacement;
      });
  KJ_LOG(INFO, "Registered exchange for reconciliation", static_cast<int>(venue));
}

void AccountReconciler::unregister_exchange(veloz::common::Venue venue) {
  auto lock = guarded_.lockExclusive();
  lock->exchanges.erase(venue);
  KJ_LOG(INFO, "Unregistered exchange from reconciliation", static_cast<int>(venue));
}

kj::Promise<void> AccountReconciler::start() {
  {
    auto lock = guarded_.lockExclusive();
    if (lock->running) {
      return kj::READY_NOW;
    }
    lock->running = true;
  }

  emit_event(ReconciliationEvent{
      .type = ReconciliationEventType::Started,
      .ts_ns = now_ns(),
      .message = kj::str("Reconciliation loop started"),
  });

  KJ_LOG(INFO, "Starting account reconciliation loop");
  return reconciliation_loop();
}

void AccountReconciler::stop() {
  {
    auto lock = guarded_.lockExclusive();
    if (!lock->running) {
      return;
    }
    lock->running = false;
  }

  // Signal the loop to stop
  stop_fulfiller_->fulfill();

  emit_event(ReconciliationEvent{
      .type = ReconciliationEventType::Completed,
      .ts_ns = now_ns(),
      .message = kj::str("Reconciliation loop stopped"),
  });

  KJ_LOG(INFO, "Stopped account reconciliation loop");
}

kj::Promise<void> AccountReconciler::reconciliation_loop() {
  auto& timer = io_context_.provider->getTimer();

  return timer.afterDelay(config_.reconciliation_interval).then([this]() -> kj::Promise<void> {
    {
      auto lock = guarded_.lockExclusive();
      if (!lock->running) {
        return kj::READY_NOW;
      }
    }

    return reconcile_now().then([this]() -> kj::Promise<void> {
      {
        auto lock = guarded_.lockExclusive();
        if (!lock->running) {
          return kj::READY_NOW;
        }
      }
      // Continue the loop
      return reconciliation_loop();
    });
  });
}

kj::Promise<void> AccountReconciler::reconcile_now() {
  auto start_time = now_ns();

  emit_event(ReconciliationEvent{
      .type = ReconciliationEventType::Started,
      .ts_ns = start_time,
      .message = kj::str("Starting reconciliation cycle"),
  });

  // Get list of venues to reconcile
  kj::Vector<veloz::common::Venue> venues;
  {
    auto lock = guarded_.lockExclusive();
    for (const auto& entry : lock->exchanges) {
      venues.add(entry.key);
    }
    lock->stats.total_reconciliations++;
  }

  // Reconcile each venue sequentially
  kj::Promise<void> chain = kj::READY_NOW;
  for (auto venue : venues) {
    chain = chain.then([this, venue]() { return reconcile_venue(venue); });
  }

  return chain.then([this, start_time]() {
    auto end_time = now_ns();
    auto duration_ns = end_time - start_time;

    {
      auto lock = guarded_.lockExclusive();
      lock->stats.successful_reconciliations++;
      lock->stats.last_reconciliation_ts_ns = end_time;
      lock->stats.last_reconciliation_duration = duration_ns * kj::NANOSECONDS;
    }

    emit_event(ReconciliationEvent{
        .type = ReconciliationEventType::Completed,
        .ts_ns = end_time,
        .message = kj::str("Reconciliation cycle completed in ", duration_ns / 1000000, "ms"),
    });
  });
}

kj::Promise<void> AccountReconciler::reconcile_symbol(veloz::common::Venue venue,
                                                      const veloz::common::SymbolId& symbol) {
  ReconciliationQueryInterface* query_interface = nullptr;
  {
    auto lock = guarded_.lockExclusive();
    KJ_IF_SOME(iface, lock->exchanges.find(venue)) {
      query_interface = iface;
    }
  }

  if (query_interface == nullptr) {
    KJ_LOG(WARNING, "No exchange registered for venue", static_cast<int>(venue));
    return kj::READY_NOW;
  }

  // Query open orders from exchange
  return query_interface->query_open_orders_async(symbol).then(
      [this, venue, symbol](kj::Vector<ExecutionReport> exchange_orders) -> kj::Promise<void> {
        // Get local pending orders
        auto local_orders = order_store_.list_pending();

        // Build map of exchange orders by client_order_id
        kj::HashMap<kj::String, ExecutionReport*> exchange_map;
        for (auto& order : exchange_orders) {
          exchange_map.upsert(kj::str(order.client_order_id), &order,
                              [](ExecutionReport*& existing, ExecutionReport*&& replacement) {
                                existing = replacement;
                              });
        }

        kj::Vector<StateMismatch> mismatches;
        kj::Vector<ExecutionReport> orphaned;

        // Compare local orders with exchange
        for (const auto& local : local_orders) {
          KJ_IF_SOME(exchange_ptr, exchange_map.find(local.client_order_id)) {
            compare_order_states(local, *exchange_ptr, mismatches);
            // Remove from map to track orphaned orders
            exchange_map.erase(local.client_order_id);
          }
          // Local order not on exchange - might be filled/cancelled
        }

        // Remaining orders in exchange_map are orphaned (on exchange but not in local state)
        for (auto& entry : exchange_map) {
          orphaned.add(kj::mv(*entry.value));
        }

        // Handle mismatches
        return handle_mismatches(venue, mismatches)
            .then([this, venue, orphaned = kj::mv(orphaned)]() mutable {
              return handle_orphaned_orders(venue, orphaned);
            });
      });
}

kj::Promise<void> AccountReconciler::reconcile_venue(veloz::common::Venue venue) {
  // For now, reconcile a default symbol - in production this would iterate over all active symbols
  veloz::common::SymbolId default_symbol("BTCUSDT"_kj);

  return reconcile_symbol(venue, default_symbol);
}

void AccountReconciler::compare_order_states(const oms::OrderState& local,
                                             const ExecutionReport& exchange,
                                             kj::Vector<StateMismatch>& mismatches) {
  // Parse local status string to OrderStatus
  OrderStatus local_status = OrderStatus::New;
  if (local.status == "ACCEPTED" || local.status == "NEW") {
    local_status = OrderStatus::Accepted;
  } else if (local.status == "PARTIALLY_FILLED") {
    local_status = OrderStatus::PartiallyFilled;
  } else if (local.status == "FILLED") {
    local_status = OrderStatus::Filled;
  } else if (local.status == "CANCELLED" || local.status == "CANCELED") {
    local_status = OrderStatus::Canceled;
  } else if (local.status == "REJECTED") {
    local_status = OrderStatus::Rejected;
  } else if (local.status == "EXPIRED") {
    local_status = OrderStatus::Expired;
  }

  // Check for status mismatch
  bool status_mismatch = (local_status != exchange.status);

  // Check for fill quantity mismatch (with tolerance for floating point)
  constexpr double qty_tolerance = 1e-8;
  bool qty_mismatch = std::abs(local.executed_qty - exchange.last_fill_qty) > qty_tolerance;

  // Check for price mismatch
  bool price_mismatch = false;
  if (local.executed_qty > 0 && exchange.last_fill_qty > 0) {
    price_mismatch = std::abs(local.avg_price - exchange.last_fill_price) > qty_tolerance;
  }

  if (status_mismatch || qty_mismatch || price_mismatch) {
    StateMismatch mismatch;
    mismatch.client_order_id = kj::str(local.client_order_id);
    mismatch.symbol = kj::str(local.symbol);
    mismatch.local_status = local_status;
    mismatch.exchange_status = exchange.status;
    mismatch.local_filled_qty = local.executed_qty;
    mismatch.exchange_filled_qty = exchange.last_fill_qty;
    mismatch.local_avg_price = local.avg_price;
    mismatch.exchange_avg_price = exchange.last_fill_price;
    mismatch.detected_ts_ns = now_ns();

    mismatches.add(kj::mv(mismatch));

    {
      auto lock = guarded_.lockExclusive();
      lock->stats.mismatches_detected++;
      lock->consecutive_mismatches++;
    }

    KJ_LOG(WARNING, "Order state mismatch detected", local.client_order_id.cStr(), "local_status",
           local.status.cStr(), "exchange_status", static_cast<int>(exchange.status));
  }
}

kj::Promise<void> AccountReconciler::handle_mismatches(veloz::common::Venue venue,
                                                       kj::Vector<StateMismatch>& mismatches) {
  if (mismatches.size() == 0) {
    // Reset consecutive mismatch counter on successful reconciliation
    auto lock = guarded_.lockExclusive();
    lock->consecutive_mismatches = 0;
    return kj::READY_NOW;
  }

  // Check if we should freeze strategy
  int consecutive_mismatches = 0;
  {
    auto lock = guarded_.lockExclusive();
    consecutive_mismatches = lock->consecutive_mismatches;
  }

  if (config_.freeze_on_mismatch &&
      consecutive_mismatches >= config_.max_mismatches_before_freeze) {
    freeze_strategy(kj::str("Too many consecutive mismatches: ", consecutive_mismatches));
  }

  // Process each mismatch
  for (auto& mismatch : mismatches) {
    emit_event(ReconciliationEvent{
        .type = ReconciliationEventType::StateMismatch,
        .ts_ns = now_ns(),
        .message = kj::str("State mismatch for order ", mismatch.client_order_id),
        .mismatch = kj::mv(mismatch),
    });

    // Update local state from exchange (trust exchange as source of truth)
    ExecutionReport exchange_state;
    exchange_state.client_order_id = kj::str(mismatch.client_order_id);
    exchange_state.status = mismatch.exchange_status;
    exchange_state.last_fill_qty = mismatch.exchange_filled_qty;
    exchange_state.last_fill_price = mismatch.exchange_avg_price;
    exchange_state.ts_recv_ns = now_ns();

    update_local_state(exchange_state);

    {
      auto lock = guarded_.lockExclusive();
      lock->stats.mismatches_corrected++;
    }

    emit_event(ReconciliationEvent{
        .type = ReconciliationEventType::OrderCorrected,
        .ts_ns = now_ns(),
        .message = kj::str("Order state corrected from exchange"),
        .client_order_id = kj::str(mismatch.client_order_id),
    });
  }

  return kj::READY_NOW;
}

kj::Promise<void> AccountReconciler::handle_orphaned_orders(veloz::common::Venue venue,
                                                            kj::Vector<ExecutionReport>& orphaned) {
  if (orphaned.size() == 0) {
    return kj::READY_NOW;
  }

  for (auto& order : orphaned) {
    {
      auto lock = guarded_.lockExclusive();
      lock->stats.orphaned_orders_found++;
    }

    emit_event(ReconciliationEvent{
        .type = ReconciliationEventType::OrphanedOrderFound,
        .ts_ns = now_ns(),
        .message = kj::str("Orphaned order found on exchange: ", order.client_order_id),
        .client_order_id = kj::str(order.client_order_id),
    });

    // Add orphaned order to local state
    update_local_state(order);

    if (config_.auto_cancel_orphaned) {
      // TODO: Cancel orphaned order via exchange adapter
      // For now, just log the event
      KJ_LOG(WARNING, "Auto-cancel orphaned order not yet implemented",
             order.client_order_id.cStr());

      emit_event(ReconciliationEvent{
          .type = ReconciliationEventType::OrderCancelled,
          .ts_ns = now_ns(),
          .message = kj::str("Orphaned order cancelled: ", order.client_order_id),
          .client_order_id = kj::str(order.client_order_id),
      });

      {
        auto lock = guarded_.lockExclusive();
        lock->stats.orphaned_orders_cancelled++;
      }
    }
  }

  return kj::READY_NOW;
}

void AccountReconciler::update_local_state(const ExecutionReport& exchange_state) {
  // Convert OrderStatus to string for OrderStore
  kj::StringPtr status_str;
  switch (exchange_state.status) {
  case OrderStatus::New:
    status_str = "NEW"_kj;
    break;
  case OrderStatus::Accepted:
    status_str = "ACCEPTED"_kj;
    break;
  case OrderStatus::PartiallyFilled:
    status_str = "PARTIALLY_FILLED"_kj;
    break;
  case OrderStatus::Filled:
    status_str = "FILLED"_kj;
    break;
  case OrderStatus::Canceled:
    status_str = "CANCELLED"_kj;
    break;
  case OrderStatus::Rejected:
    status_str = "REJECTED"_kj;
    break;
  case OrderStatus::Expired:
    status_str = "EXPIRED"_kj;
    break;
  default:
    status_str = "UNKNOWN"_kj;
    break;
  }

  order_store_.apply_order_update(exchange_state.client_order_id,
                                  kj::StringPtr(), // symbol - not always available
                                  kj::StringPtr(), // side - not always available
                                  exchange_state.venue_order_id, status_str,
                                  kj::StringPtr(), // reason
                                  exchange_state.ts_recv_ns);

  // Apply fill if there's executed quantity
  if (exchange_state.last_fill_qty > 0) {
    order_store_.apply_fill(exchange_state.client_order_id,
                            kj::StringPtr(), // symbol
                            exchange_state.last_fill_qty, exchange_state.last_fill_price,
                            exchange_state.ts_recv_ns);
  }
}

void AccountReconciler::emit_event(ReconciliationEvent event) {
  // Store in history
  {
    auto lock = guarded_.lockExclusive();
    lock->event_history.add(ReconciliationEvent{
        .type = event.type,
        .ts_ns = event.ts_ns,
        .message = kj::str(event.message),
    });

    // Trim history if too large
    if (lock->event_history.size() > lock->max_event_history) {
      // Remove oldest events
      size_t to_remove = lock->event_history.size() - lock->max_event_history;
      kj::Vector<ReconciliationEvent> new_history;
      for (size_t i = to_remove; i < lock->event_history.size(); ++i) {
        new_history.add(kj::mv(lock->event_history[i]));
      }
      lock->event_history = kj::mv(new_history);
    }
  }

  // Call callback if set
  KJ_IF_SOME(callback, event_callback_) {
    callback(event);
  }

  KJ_LOG(INFO, "Reconciliation event", to_string(event.type), event.message.cStr());
}

void AccountReconciler::freeze_strategy(kj::StringPtr reason) {
  {
    auto lock = guarded_.lockExclusive();
    if (lock->strategy_frozen) {
      return; // Already frozen
    }
    lock->strategy_frozen = true;
    lock->stats.strategy_freezes++;
  }

  emit_event(ReconciliationEvent{
      .type = ReconciliationEventType::StrategyFrozen,
      .ts_ns = now_ns(),
      .message = kj::str("Strategy frozen: ", reason),
  });

  KJ_IF_SOME(callback, freeze_callback_) {
    callback(true, reason);
  }

  KJ_LOG(WARNING, "Strategy frozen due to reconciliation issues", reason);
}

void AccountReconciler::set_event_callback(ReconciliationCallback callback) {
  event_callback_ = kj::mv(callback);
}

void AccountReconciler::set_freeze_callback(StrategyFreezeCallback callback) {
  freeze_callback_ = kj::mv(callback);
}

AccountReconciler::Stats AccountReconciler::get_stats() const {
  auto lock = guarded_.lockExclusive();
  return lock->stats;
}

bool AccountReconciler::is_strategy_frozen() const {
  auto lock = guarded_.lockExclusive();
  return lock->strategy_frozen;
}

void AccountReconciler::resume_strategy() {
  {
    auto lock = guarded_.lockExclusive();
    if (!lock->strategy_frozen) {
      return; // Not frozen
    }
    lock->strategy_frozen = false;
    lock->consecutive_mismatches = 0;
  }

  emit_event(ReconciliationEvent{
      .type = ReconciliationEventType::StrategyResumed,
      .ts_ns = now_ns(),
      .message = kj::str("Strategy resumed after manual intervention"),
  });

  KJ_IF_SOME(callback, freeze_callback_) {
    callback(false, "Manual resume"_kj);
  }

  KJ_LOG(INFO, "Strategy resumed after manual intervention");
}

kj::Vector<ReconciliationEvent> AccountReconciler::get_recent_events(size_t max_count) const {
  auto lock = guarded_.lockExclusive();
  kj::Vector<ReconciliationEvent> result;

  size_t start_idx = 0;
  if (lock->event_history.size() > max_count) {
    start_idx = lock->event_history.size() - max_count;
  }

  for (size_t i = start_idx; i < lock->event_history.size(); ++i) {
    result.add(ReconciliationEvent{
        .type = lock->event_history[i].type,
        .ts_ns = lock->event_history[i].ts_ns,
        .message = kj::str(lock->event_history[i].message),
    });
  }

  return result;
}

std::int64_t AccountReconciler::now_ns() const {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

} // namespace veloz::exec
