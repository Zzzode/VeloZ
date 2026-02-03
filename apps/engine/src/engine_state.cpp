#include "veloz/engine/engine_state.h"

#include <algorithm>

namespace veloz::engine {

EngineState::EngineState() = default;

void EngineState::init_balances() {
  std::lock_guard<std::mutex> lk(account_mu_);
  balances_.clear();
  balances_["USDT"] = Balance{.asset = "USDT", .free = 100000.0, .locked = 0.0};
  balances_["BTC"] = Balance{.asset = "BTC", .free = 0.0, .locked = 0.0};
}

std::vector<Balance> EngineState::snapshot_balances() const {
  std::vector<Balance> snapshot;
  std::lock_guard<std::mutex> lk(account_mu_);
  snapshot.reserve(balances_.size());
  if (auto it = balances_.find("USDT"); it != balances_.end()) {
    snapshot.push_back(it->second);
  }
  if (auto it = balances_.find("BTC"); it != balances_.end()) {
    snapshot.push_back(it->second);
  }
  for (const auto& [_, b] : balances_) {
    if (b.asset != "USDT" && b.asset != "BTC") {
      snapshot.push_back(b);
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

bool EngineState::has_duplicate(std::string_view client_order_id) const {
  std::lock_guard<std::mutex> lk(orders_mu_);
  return pending_.contains(std::string(client_order_id));
}

bool EngineState::reserve_for_order(const veloz::exec::PlaceOrderRequest& request,
                                    std::int64_t ts_ns, std::string& reason) {
  std::lock_guard<std::mutex> lk(account_mu_);
  auto& usdt = balances_["USDT"];
  usdt.asset = "USDT";
  auto& btc = balances_["BTC"];
  btc.asset = "BTC";
  if (request.side == veloz::exec::OrderSide::Buy) {
    const double notional = request.qty * request.price.value_or(0.0);
    if (usdt.free + 1e-12 < notional) {
      reason = "insufficient_funds";
      order_store_.note_order_params(request);
      order_store_.apply_order_update(request.client_order_id, request.symbol.value, "BUY", "",
                                      "REJECTED", reason, ts_ns);
      return false;
    }
    usdt.free -= notional;
    usdt.locked += notional;
    return true;
  }
  if (btc.free + 1e-12 < request.qty) {
    reason = "insufficient_funds";
    order_store_.note_order_params(request);
    order_store_.apply_order_update(request.client_order_id, request.symbol.value, "SELL", "",
                                    "REJECTED", reason, ts_ns);
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
  const auto risk = risk_engine_.check(request);
  if (!risk.ok) {
    decision.accepted = false;
    decision.reason = risk.reason;
    order_store_.apply_order_update(request.client_order_id, request.symbol.value,
                                    (request.side == veloz::exec::OrderSide::Sell) ? "SELL"
                                                                                  : "BUY",
                                    "", "REJECTED", risk.reason, ts_ns);
    return decision;
  }

  if (has_duplicate(request.client_order_id)) {
    decision.accepted = false;
    decision.reason = "duplicate_client_order_id";
    order_store_.apply_order_update(request.client_order_id, request.symbol.value,
                                    (request.side == veloz::exec::OrderSide::Sell) ? "SELL"
                                                                                  : "BUY",
                                    "", "REJECTED", decision.reason, ts_ns);
    return decision;
  }

  if (!reserve_for_order(request, ts_ns, decision.reason)) {
    decision.accepted = false;
    return decision;
  }

  venue_counter_++;
  decision.venue_order_id = "sim-" + std::to_string(venue_counter_);
  order_store_.apply_order_update(request.client_order_id, request.symbol.value,
                                  (request.side == veloz::exec::OrderSide::Sell) ? "SELL" : "BUY",
                                  decision.venue_order_id, "ACCEPTED", "", ts_ns);

  PendingOrder po;
  po.request = request;
  po.accept_ts_ns = ts_ns;
  po.due_fill_ts_ns = ts_ns + 300'000'000;
  po.reserved_value = (request.side == veloz::exec::OrderSide::Buy)
                          ? (request.qty * request.price.value_or(0.0))
                          : request.qty;

  {
    std::lock_guard<std::mutex> lk(orders_mu_);
    pending_.emplace(request.client_order_id, po);
  }

  decision.accepted = true;
  decision.pending = po;
  return decision;
}

CancelDecision EngineState::cancel_order(std::string_view client_order_id, std::int64_t ts_ns) {
  CancelDecision decision;
  PendingOrder cancelled;
  {
    std::lock_guard<std::mutex> lk(orders_mu_);
    auto it = pending_.find(std::string(client_order_id));
    if (it != pending_.end()) {
      cancelled = it->second;
      pending_.erase(it);
      decision.found = true;
      decision.cancelled = cancelled;
    }
  }

  if (decision.found) {
    order_store_.apply_order_update(cancelled.request.client_order_id,
                                    cancelled.request.symbol.value,
                                    (cancelled.request.side == veloz::exec::OrderSide::Sell)
                                        ? "SELL"
                                        : "BUY",
                                    "", "CANCELLED", "", ts_ns);
    release_on_cancel(cancelled);
    return decision;
  }

  decision.reason = "unknown_order";
  order_store_.apply_order_update(client_order_id, "", "", "", "REJECTED", decision.reason, ts_ns);
  return decision;
}

void EngineState::release_on_cancel(const PendingOrder& po) {
  std::lock_guard<std::mutex> lk(account_mu_);
  auto& usdt = balances_["USDT"];
  usdt.asset = "USDT";
  auto& btc = balances_["BTC"];
  btc.asset = "BTC";
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
  usdt.asset = "USDT";
  auto& btc = balances_["BTC"];
  btc.asset = "BTC";
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

std::vector<PendingOrder> EngineState::collect_due_fills(std::int64_t now_ns) {
  std::vector<PendingOrder> due;
  std::lock_guard<std::mutex> lk(orders_mu_);
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (it->second.due_fill_ts_ns <= now_ns) {
      due.push_back(it->second);
      it = pending_.erase(it);
    } else {
      ++it;
    }
  }
  return due;
}

std::optional<veloz::oms::OrderState> EngineState::get_order_state(
    std::string_view client_order_id) const {
  return order_store_.get(client_order_id);
}

}
