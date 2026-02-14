#include "veloz/engine/engine_state.h"

#include <algorithm>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <mutex>

namespace veloz::engine {

namespace {
// Helper to extract value from kj::Maybe with a default
template <typename T> T maybe_or(const kj::Maybe<T>& maybe, T default_value) {
  T result = default_value;
  KJ_IF_MAYBE (val, maybe) {
    result = *val;
  }
  return result;
}
} // namespace

EngineState::EngineState() = default;

void EngineState::init_balances() {
  std::lock_guard<std::mutex> lk(account_mu_);
  balances_.clear();
  Balance usdt;
  usdt.asset = kj::str("USDT");
  usdt.free = 100000.0;
  usdt.locked = 0.0;
  balances_["USDT"] = kj::mv(usdt);

  Balance btc;
  btc.asset = kj::str("BTC");
  btc.free = 0.0;
  btc.locked = 0.0;
  balances_["BTC"] = kj::mv(btc);
}

kj::Vector<Balance> EngineState::snapshot_balances() const {
  kj::Vector<Balance> snapshot;
  std::lock_guard<std::mutex> lk(account_mu_);

  if (auto it = balances_.find("USDT"); it != balances_.end()) {
    Balance b;
    b.asset = kj::str(it->second.asset);
    b.free = it->second.free;
    b.locked = it->second.locked;
    snapshot.add(kj::mv(b));
  }
  if (auto it = balances_.find("BTC"); it != balances_.end()) {
    Balance b;
    b.asset = kj::str(it->second.asset);
    b.free = it->second.free;
    b.locked = it->second.locked;
    snapshot.add(kj::mv(b));
  }
  for (const auto& [key, bal] : balances_) {
    if (key != "USDT" && key != "BTC") {
      Balance b;
      b.asset = kj::str(bal.asset);
      b.free = bal.free;
      b.locked = bal.locked;
      snapshot.add(kj::mv(b));
    }
  }
  return snapshot;
}

double EngineState::price() const {
  return price_.load();
}

void EngineState::set_price(double value) {
  price_.store(value);
}

bool EngineState::has_duplicate(kj::StringPtr client_order_id) const {
  std::lock_guard<std::mutex> lk(orders_mu_);
  return pending_.contains(std::string(client_order_id.cStr()));
}

bool EngineState::reserve_for_order(const veloz::exec::PlaceOrderRequest& request,
                                    std::int64_t ts_ns, kj::String& reason) {
  std::lock_guard<std::mutex> lk(account_mu_);
  auto& usdt = balances_["USDT"];
  if (usdt.asset.size() == 0) {
    usdt.asset = kj::str("USDT");
  }
  auto& btc = balances_["BTC"];
  if (btc.asset.size() == 0) {
    btc.asset = kj::str("BTC");
  }
  if (request.side == veloz::exec::OrderSide::Buy) {
    const double notional = request.qty * maybe_or(request.price, 0.0);
    if (usdt.free + 1e-12 < notional) {
      reason = kj::str("insufficient_funds");
      order_store_.note_order_params(request);
      order_store_.apply_order_update(request.client_order_id, request.symbol.value, "BUY", "",
                                      "REJECTED", reason.cStr(), ts_ns);
      return false;
    }
    usdt.free -= notional;
    usdt.locked += notional;
    return true;
  }
  if (btc.free + 1e-12 < request.qty) {
    reason = kj::str("insufficient_funds");
    order_store_.note_order_params(request);
    order_store_.apply_order_update(request.client_order_id, request.symbol.value, "SELL", "",
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
    decision.reason = kj::str(risk.reason);
    order_store_.apply_order_update(request.client_order_id, request.symbol.value,
                                    (request.side == veloz::exec::OrderSide::Sell) ? "SELL" : "BUY",
                                    "", "REJECTED", risk.reason, ts_ns);
    return decision;
  }

  if (has_duplicate(kj::StringPtr(request.client_order_id.cStr()))) {
    decision.accepted = false;
    decision.reason = kj::str("duplicate_client_order_id");
    order_store_.apply_order_update(request.client_order_id, request.symbol.value,
                                    (request.side == veloz::exec::OrderSide::Sell) ? "SELL" : "BUY",
                                    "", "REJECTED", decision.reason.cStr(), ts_ns);
    return decision;
  }

  if (!reserve_for_order(request, ts_ns, decision.reason)) {
    decision.accepted = false;
    return decision;
  }

  venue_counter_++;
  decision.venue_order_id = kj::str("sim-", venue_counter_);
  order_store_.apply_order_update(request.client_order_id, request.symbol.value,
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
    std::lock_guard<std::mutex> lk(orders_mu_);
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
    pending_.emplace(std::string(request.client_order_id.cStr()), kj::mv(po_copy));
  }

  decision.accepted = true;
  decision.pending = kj::mv(po);
  return decision;
}

CancelDecision EngineState::cancel_order(kj::StringPtr client_order_id, std::int64_t ts_ns) {
  CancelDecision decision;
  PendingOrder cancelled;
  {
    std::lock_guard<std::mutex> lk(orders_mu_);
    auto it = pending_.find(std::string(client_order_id.cStr()));
    if (it != pending_.end()) {
      // Move from map since PendingOrder contains non-copyable kj::String
      cancelled = kj::mv(it->second);
      pending_.erase(it);
      decision.found = true;
    }
  }

  if (decision.found) {
    order_store_.apply_order_update(
        cancelled.request.client_order_id, cancelled.request.symbol.value,
        (cancelled.request.side == veloz::exec::OrderSide::Sell) ? "SELL" : "BUY", "", "CANCELLED",
        "", ts_ns);
    release_on_cancel(cancelled);
    // Move cancelled to decision after using it
    decision.cancelled = kj::mv(cancelled);
    return decision;
  }

  decision.reason = kj::str("unknown_order");
  order_store_.apply_order_update(std::string(client_order_id.cStr()), "", "", "", "REJECTED",
                                  decision.reason.cStr(), ts_ns);
  return decision;
}

void EngineState::release_on_cancel(const PendingOrder& po) {
  std::lock_guard<std::mutex> lk(account_mu_);
  auto& usdt = balances_["USDT"];
  if (usdt.asset.size() == 0) {
    usdt.asset = kj::str("USDT");
  }
  auto& btc = balances_["BTC"];
  if (btc.asset.size() == 0) {
    btc.asset = kj::str("BTC");
  }
  if (po.request.side == veloz::exec::OrderSide::Buy) {
    usdt.locked -= po.reserved_value;
    usdt.free += po.reserved_value;
  } else {
    btc.locked -= po.reserved_value;
    btc.free += po.reserved_value;
  }
}

void EngineState::apply_fill(const PendingOrder& po, double fill_price, std::int64_t ts_ns) {
  order_store_.apply_fill(po.request.client_order_id, po.request.symbol.value, po.request.qty,
                          fill_price, ts_ns);
  std::lock_guard<std::mutex> lk(account_mu_);
  auto& usdt = balances_["USDT"];
  if (usdt.asset.size() == 0) {
    usdt.asset = kj::str("USDT");
  }
  auto& btc = balances_["BTC"];
  if (btc.asset.size() == 0) {
    btc.asset = kj::str("BTC");
  }
  if (po.request.side == veloz::exec::OrderSide::Buy) {
    const double notional = po.request.qty * fill_price;
    const double refund = po.reserved_value > notional ? (po.reserved_value - notional) : 0.0;
    usdt.locked -= po.reserved_value;
    usdt.free += refund;
    btc.free += po.request.qty;
  } else {
    btc.locked -= po.reserved_value;
    usdt.free += (po.request.qty * fill_price);
  }
}

kj::Vector<PendingOrder> EngineState::collect_due_fills(std::int64_t now_ns) {
  kj::Vector<PendingOrder> due;
  std::lock_guard<std::mutex> lk(orders_mu_);
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (it->second.due_fill_ts_ns <= now_ns) {
      // Move from map since PendingOrder contains non-copyable kj::String
      due.add(kj::mv(it->second));
      it = pending_.erase(it);
    } else {
      ++it;
    }
  }
  return due;
}

kj::Maybe<veloz::oms::OrderState>
EngineState::get_order_state(kj::StringPtr client_order_id) const {
  return order_store_.get(client_order_id);
}

} // namespace veloz::engine
