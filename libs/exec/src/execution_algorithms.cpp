#include "veloz/exec/execution_algorithms.h"

#include <cmath>
#include <random>

namespace veloz::exec {

// Helper to get current time in nanoseconds
static std::int64_t get_current_time_ns() {
  auto now = kj::systemPreciseMonotonicClock().now();
  return (now - kj::origin<kj::TimePoint>()) / kj::NANOSECONDS;
}

// Generate unique algorithm ID
static kj::String generate_algo_id(const char* prefix, std::size_t counter) {
  return kj::str(prefix, "-", counter, "-", get_current_time_ns());
}

// TWAP Algorithm Implementation

TwapAlgorithm::TwapAlgorithm(SmartOrderRouter& router, const veloz::common::SymbolId& symbol,
                             OrderSide side, double quantity, const TwapConfig& config)
    : router_(router) {
  auto lock = guarded_.lockExclusive();
  lock->algo_id = generate_algo_id("TWAP", 0);
  lock->symbol = symbol;
  lock->side = side;
  lock->target_qty = quantity;
  lock->config.duration = config.duration;
  lock->config.slice_interval = config.slice_interval;
  lock->config.randomization = config.randomization;
  lock->config.use_limit_orders = config.use_limit_orders;
  lock->config.limit_offset_bps = config.limit_offset_bps;
  lock->config.min_slice_qty = config.min_slice_qty;

  // Calculate total slices
  std::int64_t duration_ns = config.duration / kj::NANOSECONDS;
  std::int64_t interval_ns = config.slice_interval / kj::NANOSECONDS;
  lock->total_slices = static_cast<std::size_t>(duration_ns / interval_ns);
  if (lock->total_slices == 0) {
    lock->total_slices = 1;
  }
}

void TwapAlgorithm::start() {
  auto lock = guarded_.lockExclusive();
  if (lock->state != AlgorithmState::Pending && lock->state != AlgorithmState::Paused) {
    return;
  }

  lock->state = AlgorithmState::Running;
  lock->start_time_ns = get_current_time_ns();
  lock->last_slice_time_ns = lock->start_time_ns;
}

void TwapAlgorithm::pause() {
  auto lock = guarded_.lockExclusive();
  if (lock->state == AlgorithmState::Running) {
    lock->state = AlgorithmState::Paused;
  }
}

void TwapAlgorithm::resume() {
  auto lock = guarded_.lockExclusive();
  if (lock->state == AlgorithmState::Paused) {
    lock->state = AlgorithmState::Running;
  }
}

void TwapAlgorithm::cancel() {
  auto lock = guarded_.lockExclusive();
  if (lock->state == AlgorithmState::Running || lock->state == AlgorithmState::Paused) {
    lock->state = AlgorithmState::Cancelled;
  }
}

AlgorithmProgress TwapAlgorithm::get_progress() const {
  auto lock = guarded_.lockExclusive();

  AlgorithmProgress progress;
  progress.algo_id = kj::heapString(lock->algo_id);
  progress.type = AlgorithmType::TWAP;
  progress.state = lock->state;
  progress.target_quantity = lock->target_qty;
  progress.filled_quantity = lock->filled_qty;

  if (lock->filled_qty > 0) {
    progress.average_price = lock->total_value / lock->filled_qty;
  }

  if (lock->target_qty > 0) {
    progress.progress_pct = (lock->filled_qty / lock->target_qty) * 100.0;
  }

  progress.child_orders_sent = lock->slices_sent;

  // Count filled orders
  for (const auto& order : lock->child_orders) {
    if (order.status == OrderStatus::Filled) {
      progress.child_orders_filled++;
    }
  }

  progress.start_time_ns = lock->start_time_ns;

  if (lock->state == AlgorithmState::Running) {
    std::int64_t now_ns = get_current_time_ns();
    progress.elapsed = (now_ns - lock->start_time_ns) * kj::NANOSECONDS;

    std::int64_t duration_ns = lock->config.duration / kj::NANOSECONDS;
    std::int64_t remaining_ns = duration_ns - (now_ns - lock->start_time_ns);
    if (remaining_ns > 0) {
      progress.remaining = remaining_ns * kj::NANOSECONDS;
    }
  }

  return progress;
}

kj::Vector<ChildOrder> TwapAlgorithm::get_child_orders() const {
  auto lock = guarded_.lockExclusive();
  kj::Vector<ChildOrder> result;

  for (const auto& order : lock->child_orders) {
    ChildOrder copy;
    copy.client_order_id = kj::heapString(order.client_order_id);
    copy.venue = order.venue;
    copy.quantity = order.quantity;
    copy.price = order.price;
    copy.status = order.status;
    copy.filled_qty = order.filled_qty;
    copy.filled_price = order.filled_price;
    copy.created_at_ns = order.created_at_ns;
    copy.filled_at_ns = order.filled_at_ns;
    result.add(kj::mv(copy));
  }

  return result;
}

void TwapAlgorithm::on_tick(std::int64_t current_time_ns) {
  auto lock = guarded_.lockExclusive();

  if (lock->state != AlgorithmState::Running) {
    return;
  }

  // Check if algorithm duration has elapsed
  std::int64_t duration_ns = lock->config.duration / kj::NANOSECONDS;
  if (current_time_ns - lock->start_time_ns >= duration_ns) {
    if (lock->filled_qty >= lock->target_qty * 0.99) {
      lock->state = AlgorithmState::Completed;
    } else {
      lock->state = AlgorithmState::Failed;
    }
    return;
  }

  // Check if it's time for next slice
  std::int64_t interval_ns = lock->config.slice_interval / kj::NANOSECONDS;

  // Add randomization
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<double> dist(-lock->config.randomization,
                                              lock->config.randomization);
  double random_factor = 1.0 + dist(gen);
  std::int64_t adjusted_interval = static_cast<std::int64_t>(interval_ns * random_factor);

  if (current_time_ns - lock->last_slice_time_ns >= adjusted_interval) {
    // Need to send a slice - but we can't call send_slice while holding lock
    // Mark that we need to send
    lock->last_slice_time_ns = current_time_ns;
    auto remaining_qty = lock->target_qty - lock->filled_qty;
    auto slices_remaining = lock->total_slices - lock->slices_sent;

    if (remaining_qty > 0 && slices_remaining > 0) {
      double slice_qty = remaining_qty / static_cast<double>(slices_remaining);
      slice_qty = std::min(slice_qty, remaining_qty);

      if (slice_qty >= lock->config.min_slice_qty) {
        // Create order
        PlaceOrderRequest req;
        req.symbol = lock->symbol;
        req.side = lock->side;
        req.type = lock->config.use_limit_orders ? OrderType::Limit : OrderType::Market;
        req.qty = slice_qty;

        double price = 0.0;
        if (lock->config.use_limit_orders && lock->current_bid > 0 && lock->current_ask > 0) {
          double mid = (lock->current_bid + lock->current_ask) / 2.0;
          double offset = mid * lock->config.limit_offset_bps / 10000.0;
          price = (lock->side == OrderSide::Buy) ? mid + offset : mid - offset;
          req.price = price;
        }

        req.client_order_id = kj::str(lock->algo_id, "-", lock->slices_sent);

        // Record child order
        ChildOrder child;
        child.client_order_id = kj::heapString(req.client_order_id);
        child.quantity = slice_qty;
        child.price = price;
        child.status = OrderStatus::New;
        child.created_at_ns = current_time_ns;
        lock->child_orders.add(kj::mv(child));
        lock->slices_sent++;

        // Release lock and execute
        lock.release();
        router_.execute(req);
      }
    }
  }
}

void TwapAlgorithm::on_market_update(double bid, double ask, double volume) {
  auto lock = guarded_.lockExclusive();
  lock->current_bid = bid;
  lock->current_ask = ask;
}

void TwapAlgorithm::on_fill(kj::StringPtr client_order_id, double qty, double price) {
  auto lock = guarded_.lockExclusive();

  for (auto& order : lock->child_orders) {
    if (order.client_order_id == client_order_id) {
      order.filled_qty += qty;
      order.filled_price = price;
      order.filled_at_ns = get_current_time_ns();

      if (order.filled_qty >= order.quantity * 0.99) {
        order.status = OrderStatus::Filled;
      } else {
        order.status = OrderStatus::PartiallyFilled;
      }

      lock->filled_qty += qty;
      lock->total_value += qty * price;
      break;
    }
  }

  // Check if target reached
  if (lock->filled_qty >= lock->target_qty * 0.99) {
    lock->state = AlgorithmState::Completed;
  }
}

void TwapAlgorithm::set_progress_callback(AlgorithmCallback callback) {
  guarded_.lockExclusive()->progress_callback = kj::mv(callback);
}

void TwapAlgorithm::set_child_order_callback(ChildOrderCallback callback) {
  guarded_.lockExclusive()->child_order_callback = kj::mv(callback);
}

kj::StringPtr TwapAlgorithm::algo_id() const {
  return guarded_.lockExclusive()->algo_id;
}

void TwapAlgorithm::send_slice(std::int64_t current_time_ns) {
  // This is now handled inline in on_tick to avoid lock issues
}

double TwapAlgorithm::calculate_slice_qty() const {
  auto lock = guarded_.lockExclusive();

  double remaining_qty = lock->target_qty - lock->filled_qty;
  std::size_t remaining_slices = lock->total_slices - lock->slices_sent;

  if (remaining_slices == 0) {
    return remaining_qty;
  }

  return remaining_qty / static_cast<double>(remaining_slices);
}

// VWAP Algorithm Implementation

VwapAlgorithm::VwapAlgorithm(SmartOrderRouter& router, const veloz::common::SymbolId& symbol,
                             OrderSide side, double quantity, const VwapConfig& config)
    : router_(router) {
  auto lock = guarded_.lockExclusive();
  lock->algo_id = generate_algo_id("VWAP", 0);
  lock->symbol = symbol;
  lock->side = side;
  lock->target_qty = quantity;
  lock->config.duration = config.duration;
  lock->config.slice_interval = config.slice_interval;
  lock->config.participation_rate = config.participation_rate;
  lock->config.use_limit_orders = config.use_limit_orders;
  lock->config.limit_offset_bps = config.limit_offset_bps;

  // Copy volume profile
  for (const auto& v : config.volume_profile) {
    lock->config.volume_profile.add(v);
  }

  // Calculate total slices
  std::int64_t duration_ns = config.duration / kj::NANOSECONDS;
  std::int64_t interval_ns = config.slice_interval / kj::NANOSECONDS;
  lock->total_slices = static_cast<std::size_t>(duration_ns / interval_ns);
  if (lock->total_slices == 0) {
    lock->total_slices = 1;
  }

  // Generate default volume profile if not provided
  if (lock->config.volume_profile.empty()) {
    for (std::size_t i = 0; i < lock->total_slices; ++i) {
      lock->config.volume_profile.add(1.0 / static_cast<double>(lock->total_slices));
    }
  }
}

void VwapAlgorithm::start() {
  auto lock = guarded_.lockExclusive();
  if (lock->state != AlgorithmState::Pending && lock->state != AlgorithmState::Paused) {
    return;
  }

  lock->state = AlgorithmState::Running;
  lock->start_time_ns = get_current_time_ns();
  lock->last_slice_time_ns = lock->start_time_ns;
}

void VwapAlgorithm::pause() {
  auto lock = guarded_.lockExclusive();
  if (lock->state == AlgorithmState::Running) {
    lock->state = AlgorithmState::Paused;
  }
}

void VwapAlgorithm::resume() {
  auto lock = guarded_.lockExclusive();
  if (lock->state == AlgorithmState::Paused) {
    lock->state = AlgorithmState::Running;
  }
}

void VwapAlgorithm::cancel() {
  auto lock = guarded_.lockExclusive();
  if (lock->state == AlgorithmState::Running || lock->state == AlgorithmState::Paused) {
    lock->state = AlgorithmState::Cancelled;
  }
}

AlgorithmProgress VwapAlgorithm::get_progress() const {
  auto lock = guarded_.lockExclusive();

  AlgorithmProgress progress;
  progress.algo_id = kj::heapString(lock->algo_id);
  progress.type = AlgorithmType::VWAP;
  progress.state = lock->state;
  progress.target_quantity = lock->target_qty;
  progress.filled_quantity = lock->filled_qty;

  if (lock->filled_qty > 0) {
    progress.average_price = lock->total_value / lock->filled_qty;
  }

  if (lock->target_qty > 0) {
    progress.progress_pct = (lock->filled_qty / lock->target_qty) * 100.0;
  }

  progress.child_orders_sent = lock->slices_sent;

  for (const auto& order : lock->child_orders) {
    if (order.status == OrderStatus::Filled) {
      progress.child_orders_filled++;
    }
  }

  progress.start_time_ns = lock->start_time_ns;

  return progress;
}

kj::Vector<ChildOrder> VwapAlgorithm::get_child_orders() const {
  auto lock = guarded_.lockExclusive();
  kj::Vector<ChildOrder> result;

  for (const auto& order : lock->child_orders) {
    ChildOrder copy;
    copy.client_order_id = kj::heapString(order.client_order_id);
    copy.venue = order.venue;
    copy.quantity = order.quantity;
    copy.price = order.price;
    copy.status = order.status;
    copy.filled_qty = order.filled_qty;
    copy.filled_price = order.filled_price;
    result.add(kj::mv(copy));
  }

  return result;
}

void VwapAlgorithm::on_tick(std::int64_t current_time_ns) {
  auto lock = guarded_.lockExclusive();

  if (lock->state != AlgorithmState::Running) {
    return;
  }

  std::int64_t duration_ns = lock->config.duration / kj::NANOSECONDS;
  if (current_time_ns - lock->start_time_ns >= duration_ns) {
    if (lock->filled_qty >= lock->target_qty * 0.99) {
      lock->state = AlgorithmState::Completed;
    } else {
      lock->state = AlgorithmState::Failed;
    }
    return;
  }

  std::int64_t interval_ns = lock->config.slice_interval / kj::NANOSECONDS;
  if (current_time_ns - lock->last_slice_time_ns >= interval_ns) {
    lock->last_slice_time_ns = current_time_ns;

    double remaining_qty = lock->target_qty - lock->filled_qty;
    if (remaining_qty <= 0) {
      lock->state = AlgorithmState::Completed;
      return;
    }

    // Calculate slice quantity based on volume profile
    double slice_qty = 0.0;
    if (lock->slices_sent < lock->config.volume_profile.size()) {
      slice_qty = lock->target_qty * lock->config.volume_profile[lock->slices_sent];
    }
    slice_qty = std::min(slice_qty, remaining_qty);

    if (slice_qty > 0) {
      PlaceOrderRequest req;
      req.symbol = lock->symbol;
      req.side = lock->side;
      req.type = lock->config.use_limit_orders ? OrderType::Limit : OrderType::Market;
      req.qty = slice_qty;

      double price = 0.0;
      if (lock->config.use_limit_orders && lock->current_bid > 0 && lock->current_ask > 0) {
        double mid = (lock->current_bid + lock->current_ask) / 2.0;
        double offset = mid * lock->config.limit_offset_bps / 10000.0;
        price = (lock->side == OrderSide::Buy) ? mid + offset : mid - offset;
        req.price = price;
      }

      req.client_order_id = kj::str(lock->algo_id, "-", lock->slices_sent);

      ChildOrder child;
      child.client_order_id = kj::heapString(req.client_order_id);
      child.quantity = slice_qty;
      child.price = price;
      child.status = OrderStatus::New;
      child.created_at_ns = current_time_ns;
      lock->child_orders.add(kj::mv(child));
      lock->slices_sent++;

      lock.release();
      router_.execute(req);
    }
  }
}

void VwapAlgorithm::on_market_update(double bid, double ask, double volume) {
  auto lock = guarded_.lockExclusive();
  lock->current_bid = bid;
  lock->current_ask = ask;
  lock->cumulative_volume += volume;
}

void VwapAlgorithm::on_fill(kj::StringPtr client_order_id, double qty, double price) {
  auto lock = guarded_.lockExclusive();

  for (auto& order : lock->child_orders) {
    if (order.client_order_id == client_order_id) {
      order.filled_qty += qty;
      order.filled_price = price;

      if (order.filled_qty >= order.quantity * 0.99) {
        order.status = OrderStatus::Filled;
      } else {
        order.status = OrderStatus::PartiallyFilled;
      }

      lock->filled_qty += qty;
      lock->total_value += qty * price;
      break;
    }
  }

  if (lock->filled_qty >= lock->target_qty * 0.99) {
    lock->state = AlgorithmState::Completed;
  }
}

void VwapAlgorithm::set_progress_callback(AlgorithmCallback callback) {
  guarded_.lockExclusive()->progress_callback = kj::mv(callback);
}

void VwapAlgorithm::set_child_order_callback(ChildOrderCallback callback) {
  guarded_.lockExclusive()->child_order_callback = kj::mv(callback);
}

kj::StringPtr VwapAlgorithm::algo_id() const {
  return guarded_.lockExclusive()->algo_id;
}

void VwapAlgorithm::send_slice(std::int64_t current_time_ns) {
  // Handled inline in on_tick
}

double VwapAlgorithm::calculate_slice_qty(std::size_t slice_index) const {
  auto lock = guarded_.lockExclusive();

  if (slice_index >= lock->config.volume_profile.size()) {
    return 0.0;
  }

  return lock->target_qty * lock->config.volume_profile[slice_index];
}

// Algorithm Manager Implementation

AlgorithmManager::AlgorithmManager(SmartOrderRouter& router) : router_(router) {}

kj::String AlgorithmManager::start_twap(const veloz::common::SymbolId& symbol, OrderSide side,
                                        double quantity, const TwapConfig& config) {
  auto lock = guarded_.lockExclusive();

  auto algo = kj::heap<TwapAlgorithm>(router_, symbol, side, quantity, config);
  kj::String algo_id = kj::heapString(algo->algo_id());

  algo->start();

  lock->algo_symbols.insert(kj::heapString(algo_id), symbol);
  lock->algorithms.insert(kj::heapString(algo_id), kj::mv(algo));
  lock->algo_counter++;

  return algo_id;
}

kj::String AlgorithmManager::start_vwap(const veloz::common::SymbolId& symbol, OrderSide side,
                                        double quantity, const VwapConfig& config) {
  auto lock = guarded_.lockExclusive();

  auto algo = kj::heap<VwapAlgorithm>(router_, symbol, side, quantity, config);
  kj::String algo_id = kj::heapString(algo->algo_id());

  algo->start();

  lock->algo_symbols.insert(kj::heapString(algo_id), symbol);
  lock->algorithms.insert(kj::heapString(algo_id), kj::mv(algo));
  lock->algo_counter++;

  return algo_id;
}

void AlgorithmManager::pause(kj::StringPtr algo_id) {
  auto lock = guarded_.lockExclusive();
  KJ_IF_SOME(algo, lock->algorithms.find(algo_id)) {
    algo->pause();
  }
}

void AlgorithmManager::resume(kj::StringPtr algo_id) {
  auto lock = guarded_.lockExclusive();
  KJ_IF_SOME(algo, lock->algorithms.find(algo_id)) {
    algo->resume();
  }
}

void AlgorithmManager::cancel(kj::StringPtr algo_id) {
  auto lock = guarded_.lockExclusive();
  KJ_IF_SOME(algo, lock->algorithms.find(algo_id)) {
    algo->cancel();
  }
}

kj::Maybe<AlgorithmProgress> AlgorithmManager::get_progress(kj::StringPtr algo_id) const {
  auto lock = guarded_.lockExclusive();
  KJ_IF_SOME(algo, lock->algorithms.find(algo_id)) {
    return algo->get_progress();
  }
  return kj::none;
}

kj::Vector<AlgorithmProgress> AlgorithmManager::get_all_progress() const {
  auto lock = guarded_.lockExclusive();
  kj::Vector<AlgorithmProgress> result;

  for (const auto& entry : lock->algorithms) {
    result.add(entry.value->get_progress());
  }

  return result;
}

void AlgorithmManager::on_tick(std::int64_t current_time_ns) {
  auto lock = guarded_.lockExclusive();

  for (auto& entry : lock->algorithms) {
    entry.value->on_tick(current_time_ns);
  }
}

void AlgorithmManager::on_market_update(const veloz::common::SymbolId& symbol, double bid,
                                        double ask, double volume) {
  auto lock = guarded_.lockExclusive();

  for (auto& entry : lock->algorithms) {
    KJ_IF_SOME(algo_symbol, lock->algo_symbols.find(entry.key)) {
      if (algo_symbol.value == symbol.value) {
        entry.value->on_market_update(bid, ask, volume);
      }
    }
  }
}

void AlgorithmManager::on_fill(kj::StringPtr client_order_id, double qty, double price) {
  auto lock = guarded_.lockExclusive();

  // Find which algorithm owns this child order
  for (auto& entry : lock->algorithms) {
    auto child_orders = entry.value->get_child_orders();
    for (const auto& child : child_orders) {
      if (child.client_order_id == client_order_id) {
        entry.value->on_fill(client_order_id, qty, price);
        return;
      }
    }
  }
}

void AlgorithmManager::set_progress_callback(AlgorithmCallback callback) {
  guarded_.lockExclusive()->progress_callback = kj::mv(callback);
}

void AlgorithmManager::set_child_order_callback(ChildOrderCallback callback) {
  guarded_.lockExclusive()->child_order_callback = kj::mv(callback);
}

void AlgorithmManager::cleanup_completed() {
  auto lock = guarded_.lockExclusive();

  kj::Vector<kj::String> to_remove;

  for (const auto& entry : lock->algorithms) {
    auto progress = entry.value->get_progress();
    if (progress.state == AlgorithmState::Completed ||
        progress.state == AlgorithmState::Cancelled || progress.state == AlgorithmState::Failed) {
      to_remove.add(kj::heapString(entry.key));
    }
  }

  for (const auto& key : to_remove) {
    lock->algorithms.erase(key);
    lock->algo_symbols.erase(key);
  }
}

} // namespace veloz::exec
