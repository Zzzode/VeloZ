#include "veloz/engine/engine_state.h"

#include <kj/common.h>
#include <kj/debug.h>
#include <kj/hash.h>
#include <kj/map.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::engine {

namespace {
// Helper to extract value from kj::Maybe with a default
template <typename T> T maybe_or(const kj::Maybe<T>& maybe, T default_value) {
  T result = default_value;
  KJ_IF_SOME (val, maybe) {
    result = val;
  }
  return result;
}
} // namespace

EngineState::EngineState() = default;

void EngineState::init_balances() {
  auto lock = balances_.lockExclusive();
  lock->clear();

  Balance usdt;
  usdt.asset = kj::str("USDT");
  usdt.free = 100000.0;
  usdt.locked = 0.0;
  lock->insert(kj::str("USDT"), kj::mv(usdt));

  Balance btc;
  btc.asset = kj::str("BTC");
  btc.free = 0.0;
  btc.locked = 0.0;
  lock->insert(kj::str("BTC"), kj::mv(btc));
}

kj::Vector<Balance> EngineState::snapshot_balances() const {
  kj::Vector<Balance> snapshot;
  auto lock = balances_.lockShared();

  KJ_IF_SOME (usdt, lock->find("USDT"_kj)) {
    Balance b;
    b.asset = kj::str(usdt.asset);
    b.free = usdt.free;
    b.locked = usdt.locked;
    snapshot.add(kj::mv(b));
  }
  KJ_IF_SOME (btc, lock->find("BTC"_kj)) {
    Balance b;
    b.asset = kj::str(btc.asset);
    b.free = btc.free;
    b.locked = btc.locked;
    snapshot.add(kj::mv(b));
  }
  for (auto& entry : *lock) {
    if (entry.key != "USDT"_kj && entry.key != "BTC"_kj) {
      Balance b;
      b.asset = kj::str(entry.value.asset);
      b.free = entry.value.free;
      b.locked = entry.value.locked;
      snapshot.add(kj::mv(b));
    }
  }
  return snapshot;
}

double EngineState::price() const {
  return *price_.lockShared();
}

void EngineState::set_price(double value) {
  *price_.lockExclusive() = value;
}

bool EngineState::has_duplicate(kj::StringPtr client_order_id) const {
  auto lock = pending_.lockShared();
  return lock->find(client_order_id) != kj::none;
}

bool EngineState::reserve_for_order(const veloz::exec::PlaceOrderRequest& request,
                                    std::int64_t ts_ns, kj::String& reason) {
  auto lock = balances_.lockExclusive();

  // Ensure USDT balance exists
  KJ_IF_SOME (usdt, lock->find("USDT"_kj)) {
    (void)usdt; // exists
  } else {
    Balance usdt;
    usdt.asset = kj::str("USDT");
    usdt.free = 0.0;
    usdt.locked = 0.0;
    lock->insert(kj::str("USDT"), kj::mv(usdt));
  }

  // Ensure BTC balance exists
  KJ_IF_SOME (btc, lock->find("BTC"_kj)) {
    (void)btc; // exists
  } else {
    Balance btc;
    btc.asset = kj::str("BTC");
    btc.free = 0.0;
    btc.locked = 0.0;
    lock->insert(kj::str("BTC"), kj::mv(btc));
  }

  if (request.side == veloz::exec::OrderSide::Buy) {
    const double notional = request.qty * maybe_or(request.price, 0.0);
    auto& usdt = KJ_ASSERT_NONNULL(lock->find("USDT"_kj));
    if (usdt.free + 1e-12 < notional) {
      reason = kj::str("insufficient_funds");
      order_store_.note_order_params(request);
      order_store_.apply_order_update(request.client_order_id, request.symbol.value.c_str(), "BUY", "",
                                      "REJECTED", reason.cStr(), ts_ns);
      return false;
    }
    usdt.free -= notional;
    usdt.locked += notional;
    return true;
  }

  auto& btc = KJ_ASSERT_NONNULL(lock->find("BTC"_kj));
  if (btc.free + 1e-12 < request.qty) {
    reason = kj::str("insufficient_funds");
    order_store_.note_order_params(request);
    order_store_.apply_order_update(request.client_order_id, request.symbol.value.c_str(), "SELL", "",
                                    "REJECTED", reason.cStr(), ts_ns);
    return false;
  }
  btc.free -= request.qty;
  btc.locked += request.qty;
  return true;
}

OrderDecision EngineState::place_order(const veloz::exec::PlaceOrderRequest& request,
                                       std::int64_t ts_ns) {
  OrderDecision decision;
  order_store_.note_order_params(request);
  const auto risk = risk_engine_.check_pre_trade(request);
  if (!risk.allowed) {
    decision.accepted = false;
    decision.reason = kj::str(risk.reason.cStr());
    order_store_.apply_order_update(request.client_order_id, request.symbol.value.c_str(),
                                    (request.side == veloz::exec::OrderSide::Sell) ? "SELL" : "BUY",
                                    "", "REJECTED", risk.reason.cStr(), ts_ns);
    return decision;
  }

  if (has_duplicate(kj::StringPtr(request.client_order_id.cStr()))) {
    decision.accepted = false;
    decision.reason = kj::str("duplicate_client_order_id");
    order_store_.apply_order_update(request.client_order_id, request.symbol.value.c_str(),
                                    (request.side == veloz::exec::OrderSide::Sell) ? "SELL" : "BUY",
                                    "", "REJECTED", decision.reason.cStr(), ts_ns);
    return decision;
  }

  if (!reserve_for_order(request, ts_ns, decision.reason)) {
    decision.accepted = false;
    return decision;
  }

  std::uint64_t counter;
  {
    auto lock = venue_counter_.lockExclusive();
    (*lock)++;
    counter = *lock;
  }
  decision.venue_order_id = kj::str("sim-", counter);
  order_store_.apply_order_update(request.client_order_id, request.symbol.value.c_str(),
                                  (request.side == veloz::exec::OrderSide::Sell) ? "SELL" : "BUY",
                                  decision.venue_order_id.cStr(), "ACCEPTED", "", ts_ns);

  PendingOrder po;
  // Copy request fields manually since PlaceOrderRequest contains non-copyable kj::String
  po.request.symbol = request.symbol;
  po.request.side = request.side;
  po.request.type = request.type;
  po.request.tif = request.tif;
  po.request.qty = request.qty;
  po.request.price = request.price;
  po.request.client_order_id = kj::heapString(request.client_order_id);
  po.request.reduce_only = request.reduce_only;
  po.request.post_only = request.post_only;
  po.accept_ts_ns = ts_ns;
  po.due_fill_ts_ns = ts_ns + 300'000'000;
  po.reserved_value = (request.side == veloz::exec::OrderSide::Buy)
                          ? (request.qty * maybe_or(request.price, 0.0))
                          : request.qty;

  {
    auto lock = pending_.lockExclusive();
    // Create a copy for the map since PendingOrder contains non-copyable kj::String
    PendingOrder po_copy;
    po_copy.request.symbol = po.request.symbol;
    po_copy.request.side = po.request.side;
    po_copy.request.type = po.request.type;
    po_copy.request.tif = po.request.tif;
    po_copy.request.qty = po.request.qty;
    po_copy.request.price = po.request.price;
    po_copy.request.client_order_id = kj::heapString(po.request.client_order_id);
    po_copy.request.reduce_only = po.request.reduce_only;
    po_copy.request.post_only = po.request.post_only;
    po_copy.accept_ts_ns = po.accept_ts_ns;
    po_copy.due_fill_ts_ns = po.due_fill_ts_ns;
    po_copy.reserved_value = po.reserved_value;
    lock->insert(kj::heapString(request.client_order_id), kj::mv(po_copy));
  }

  decision.accepted = true;
  decision.pending = kj::mv(po);
  return decision;
}

CancelDecision EngineState::cancel_order(kj::StringPtr client_order_id, std::int64_t ts_ns) {
  CancelDecision decision;
  PendingOrder cancelled;
  {
    auto lock = pending_.lockExclusive();
    KJ_IF_SOME (po, lock->find(client_order_id)) {
      // Copy fields since we need to use it after erasing
      cancelled.request.symbol = po.request.symbol;
      cancelled.request.side = po.request.side;
      cancelled.request.type = po.request.type;
      cancelled.request.tif = po.request.tif;
      cancelled.request.qty = po.request.qty;
      cancelled.request.price = po.request.price;
      cancelled.request.client_order_id = kj::heapString(po.request.client_order_id);
      cancelled.request.reduce_only = po.request.reduce_only;
      cancelled.request.post_only = po.request.post_only;
      cancelled.accept_ts_ns = po.accept_ts_ns;
      cancelled.due_fill_ts_ns = po.due_fill_ts_ns;
      cancelled.reserved_value = po.reserved_value;
      lock->erase(client_order_id);
      decision.found = true;
    }
  }

  if (decision.found) {
    order_store_.apply_order_update(
        cancelled.request.client_order_id, cancelled.request.symbol.value.c_str(),
        (cancelled.request.side == veloz::exec::OrderSide::Sell) ? "SELL" : "BUY", "", "CANCELLED",
        "", ts_ns);
    release_on_cancel(cancelled);
    // Move cancelled to decision after using it
    decision.cancelled = kj::mv(cancelled);
    return decision;
  }

  decision.reason = kj::str("unknown_order");
  order_store_.apply_order_update(client_order_id, "", "", "", "REJECTED",
                                  decision.reason.cStr(), ts_ns);
  return decision;
}

void EngineState::release_on_cancel(const PendingOrder& po) {
  auto lock = balances_.lockExclusive();

  // Ensure USDT balance exists
  KJ_IF_SOME (usdt, lock->find("USDT"_kj)) {
    (void)usdt;
  } else {
    Balance usdt;
    usdt.asset = kj::str("USDT");
    usdt.free = 0.0;
    usdt.locked = 0.0;
    lock->insert(kj::str("USDT"), kj::mv(usdt));
  }

  // Ensure BTC balance exists
  KJ_IF_SOME (btc, lock->find("BTC"_kj)) {
    (void)btc;
  } else {
    Balance btc;
    btc.asset = kj::str("BTC");
    btc.free = 0.0;
    btc.locked = 0.0;
    lock->insert(kj::str("BTC"), kj::mv(btc));
  }

  if (po.request.side == veloz::exec::OrderSide::Buy) {
    auto& usdt = KJ_ASSERT_NONNULL(lock->find("USDT"_kj));
    usdt.locked -= po.reserved_value;
    usdt.free += po.reserved_value;
  } else {
    auto& btc = KJ_ASSERT_NONNULL(lock->find("BTC"_kj));
    btc.locked -= po.reserved_value;
    btc.free += po.reserved_value;
  }
}

void EngineState::apply_fill(const PendingOrder& po, double fill_price, std::int64_t ts_ns) {
  order_store_.apply_fill(po.request.client_order_id, po.request.symbol.value.c_str(), po.request.qty,
                          fill_price, ts_ns);
  auto lock = balances_.lockExclusive();

  // Ensure USDT balance exists
  KJ_IF_SOME (usdt, lock->find("USDT"_kj)) {
    (void)usdt;
  } else {
    Balance usdt;
    usdt.asset = kj::str("USDT");
    usdt.free = 0.0;
    usdt.locked = 0.0;
    lock->insert(kj::str("USDT"), kj::mv(usdt));
  }

  // Ensure BTC balance exists
  KJ_IF_SOME (btc, lock->find("BTC"_kj)) {
    (void)btc;
  } else {
    Balance btc;
    btc.asset = kj::str("BTC");
    btc.free = 0.0;
    btc.locked = 0.0;
    lock->insert(kj::str("BTC"), kj::mv(btc));
  }

  if (po.request.side == veloz::exec::OrderSide::Buy) {
    const double notional = po.request.qty * fill_price;
    const double refund = po.reserved_value > notional ? (po.reserved_value - notional) : 0.0;
    auto& usdt = KJ_ASSERT_NONNULL(lock->find("USDT"_kj));
    auto& btc = KJ_ASSERT_NONNULL(lock->find("BTC"_kj));
    usdt.locked -= po.reserved_value;
    usdt.free += refund;
    btc.free += po.request.qty;
  } else {
    auto& usdt = KJ_ASSERT_NONNULL(lock->find("USDT"_kj));
    auto& btc = KJ_ASSERT_NONNULL(lock->find("BTC"_kj));
    btc.locked -= po.reserved_value;
    usdt.free += (po.request.qty * fill_price);
  }
}

kj::Vector<PendingOrder> EngineState::collect_due_fills(std::int64_t now_ns) {
  kj::Vector<PendingOrder> due;
  auto lock = pending_.lockExclusive();

  // Collect keys of due orders first
  kj::Vector<kj::String> keys_to_remove;
  for (auto& entry : *lock) {
    if (entry.value.due_fill_ts_ns <= now_ns) {
      keys_to_remove.add(kj::heapString(entry.key));
    }
  }

  // Now remove and collect
  for (auto& key : keys_to_remove) {
    KJ_IF_SOME (po, lock->find(key)) {
      PendingOrder copy;
      copy.request.symbol = po.request.symbol;
      copy.request.side = po.request.side;
      copy.request.type = po.request.type;
      copy.request.tif = po.request.tif;
      copy.request.qty = po.request.qty;
      copy.request.price = po.request.price;
      copy.request.client_order_id = kj::heapString(po.request.client_order_id);
      copy.request.reduce_only = po.request.reduce_only;
      copy.request.post_only = po.request.post_only;
      copy.accept_ts_ns = po.accept_ts_ns;
      copy.due_fill_ts_ns = po.due_fill_ts_ns;
      copy.reserved_value = po.reserved_value;
      due.add(kj::mv(copy));
      lock->erase(key);
    }
  }
  return due;
}

kj::Maybe<veloz::oms::OrderState>
EngineState::get_order_state(kj::StringPtr client_order_id) const {
  return order_store_.get(client_order_id);
}

} // namespace veloz::engine
