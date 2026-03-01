#include "engine_bridge.h"

#include "subprocess.h"

// Note: For initial implementation, EngineBridge operates independently
// When full integration with veloz::engine is needed, include:
// #include "veloz/engine/command_parser.h"
// #include "veloz/engine/engine_app.h"

#include <chrono>
#include <iomanip>
#include <kj/async.h>
#include <kj/debug.h>
#include <kj/filesystem.h>
#include <kj/one-of.h>
#include <kj/time.h>
#include <sstream>
#include <veloz/core/json.h>

namespace veloz::gateway::bridge {

kj::String format_order_command(kj::StringPtr side, kj::StringPtr symbol, double qty, double price,
                                kj::StringPtr client_id);
kj::String format_cancel_command(kj::StringPtr client_id);
kj::Maybe<BridgeEvent> parse_ndjson_event(kj::StringPtr line);

// ============================================================================
// BridgeEvent helper methods
// ============================================================================

namespace {

// Get current timestamp in nanoseconds
std::int64_t now_ns() {
  auto now = std::chrono::steady_clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
  return ns.count();
}

kj::Maybe<kj::String> resolve_default_engine_binary() {
  auto fs = kj::newDiskFilesystem();
  auto base = fs->getCurrentPath();
  auto& root = fs->getRoot();

  auto try_candidate = [&](kj::StringPtr candidate) -> kj::Maybe<kj::String> {
    auto resolved = base.eval(candidate);
    if (root.exists(resolved)) {
      return resolved.toString(true);
    }
    return kj::none;
  };

  KJ_IF_SOME(path, try_candidate("build/dev/apps/engine/veloz_engine")) {
    return kj::mv(path);
  }
  KJ_IF_SOME(path, try_candidate("../build/dev/apps/engine/veloz_engine")) {
    return kj::mv(path);
  }
  KJ_IF_SOME(path, try_candidate("../../build/dev/apps/engine/veloz_engine")) {
    return kj::mv(path);
  }
  KJ_IF_SOME(path, try_candidate("apps/engine/veloz_engine")) {
    return kj::mv(path);
  }
  KJ_IF_SOME(path, try_candidate("../apps/engine/veloz_engine")) {
    return kj::mv(path);
  }
  KJ_IF_SOME(path, try_candidate("../engine/veloz_engine")) {
    return kj::mv(path);
  }
  KJ_IF_SOME(path, try_candidate("../../engine/veloz_engine")) {
    return kj::mv(path);
  }

  return kj::none;
}

// OrderState conversion helper
KJ_UNUSED BridgeEvent create_order_event(veloz::oms::OrderState&& state) {
  BridgeEvent event;
  event.type = BridgeEventType::OrderUpdate;
  event.timestamp_ns = now_ns();

  BridgeEvent::OrderUpdateData data;
  data.order_state = kj::mv(state); // Move the state
  event.data.init<BridgeEvent::OrderUpdateData>(kj::mv(data));

  return event;
}

KJ_UNUSED BridgeEvent create_fill_event(kj::StringPtr client_order_id, kj::StringPtr symbol,
                                        double qty, double price, std::int64_t ts_ns) {
  BridgeEvent event;
  event.type = BridgeEventType::Fill;
  event.timestamp_ns = now_ns();

  BridgeEvent::FillData data;
  data.client_order_id = kj::heapString(client_order_id);
  data.symbol = kj::heapString(symbol);
  data.qty = qty;
  data.price = price;
  data.ts_ns = ts_ns;
  event.data.init<BridgeEvent::FillData>(kj::mv(data));

  return event;
}

KJ_UNUSED BridgeEvent create_error_event(kj::StringPtr message) {
  BridgeEvent event;
  event.type = BridgeEventType::Error;
  event.timestamp_ns = now_ns();

  BridgeEvent::ErrorData data;
  data.message = kj::heapString(message);
  data.ts_ns = now_ns();
  event.data.init<BridgeEvent::ErrorData>(kj::mv(data));

  return event;
}

veloz::oms::OrderState clone_order_state(const veloz::oms::OrderState& source) {
  veloz::oms::OrderState copy;
  copy.client_order_id = kj::str(source.client_order_id);
  copy.symbol = kj::str(source.symbol);
  copy.side = kj::str(source.side);
  copy.order_qty = source.order_qty;
  copy.limit_price = source.limit_price;
  copy.executed_qty = source.executed_qty;
  copy.avg_price = source.avg_price;
  copy.venue_order_id = kj::str(source.venue_order_id);
  copy.status = kj::str(source.status);
  copy.reason = kj::str(source.reason);
  copy.last_ts_ns = source.last_ts_ns;
  copy.created_ts_ns = source.created_ts_ns;
  return copy;
}

MarketSnapshot clone_market_snapshot(const MarketSnapshot& source) {
  MarketSnapshot copy;
  copy.symbol = kj::str(source.symbol);
  copy.best_bid_price = source.best_bid_price;
  copy.best_bid_qty = source.best_bid_qty;
  copy.best_ask_price = source.best_ask_price;
  copy.best_ask_qty = source.best_ask_qty;
  copy.last_price = source.last_price;
  copy.volume_24h = source.volume_24h;
  copy.last_trade_id = source.last_trade_id;
  copy.last_update_ns = source.last_update_ns;
  copy.exchange_ts_ns = source.exchange_ts_ns;
  return copy;
}

AccountState clone_account_state(const AccountState& source) {
  AccountState copy;
  copy.total_equity = source.total_equity;
  copy.available_balance = source.available_balance;
  copy.unrealized_pnl = source.unrealized_pnl;
  copy.open_position_count = source.open_position_count;
  copy.total_position_notional = source.total_position_notional;
  copy.last_update_ns = source.last_update_ns;
  for (const auto& entry : source.balances) {
    copy.balances.insert(kj::str(entry.key), entry.value);
  }
  return copy;
}

MarketSnapshot default_market_snapshot(kj::StringPtr symbol) {
  MarketSnapshot snapshot;
  snapshot.symbol = kj::str(symbol);
  snapshot.last_update_ns = now_ns();
  snapshot.exchange_ts_ns = snapshot.last_update_ns;
  return snapshot;
}

AccountState default_account_state() {
  AccountState state;
  state.last_update_ns = now_ns();
  return state;
}

bool is_terminal_status(kj::StringPtr status) {
  return status == "filled"_kj || status == "canceled"_kj || status == "cancelled"_kj ||
         status == "rejected"_kj;
}

} // namespace

// ============================================================================
// EngineBridge Implementation
// ============================================================================

void EngineBridge::TaskErrorHandler::taskFailed(kj::Exception&& exception) {
  KJ_LOG(ERROR, "EngineBridge task failed", exception);
}

EngineBridge::EngineBridge(EngineBridgeConfig config)
    : config_(kj::mv(config)), event_queue_(), state_(), metrics_(), pending_stdout_(kj::str("")) {
  KJ_REQUIRE(config_.event_queue_capacity > 0, "Event queue capacity must be positive");
  KJ_REQUIRE(config_.max_subscriptions > 0, "Max subscriptions must be positive");
}

EngineBridge::~EngineBridge() {
  stop();
}

// ============================================================================
// Engine Lifecycle
// ============================================================================

kj::Promise<void> EngineBridge::initialize(kj::AsyncIoContext& io) {
  KJ_REQUIRE(!running_.load(std::memory_order_acquire), "EngineBridge already initialized");

  io_context_ = &io;

  // Spawn engine subprocess
  auto subprocess = kj::heap<SubprocessHandle>(io);

  // Build engine command
  kj::Vector<kj::StringPtr> args;
  args.add("--stdio"_kj);

  kj::Maybe<kj::String> resolved_engine_path;
  kj::StringPtr engine_path_ptr;
  KJ_IF_SOME(engine_path, config_.engine_binary_path) {
    engine_path_ptr = engine_path;
  }
  else {
    resolved_engine_path = resolve_default_engine_binary();
    KJ_IF_SOME(path, resolved_engine_path) {
      engine_path_ptr = path;
    }
    else {
      engine_path_ptr = "build/dev/apps/engine/veloz_engine"_kj;
    }
  }

  co_await subprocess->spawn(engine_path_ptr, args.releaseAsArray());

  engine_subprocess_ = kj::mv(subprocess);

  KJ_LOG(INFO, "EngineBridge initialized with subprocess");
  co_return;
}

kj::Promise<void> EngineBridge::start() {
  KJ_REQUIRE(io_context_ != nullptr, "EngineBridge not initialized");
  KJ_REQUIRE(!running_.load(std::memory_order_acquire), "EngineBridge already running");
  KJ_REQUIRE(engine_subprocess_ != kj::none, "Engine subprocess not initialized");

  running_.store(true, std::memory_order_release);

  event_processor_task_ = kj::heap<kj::TaskSet>(task_error_handler_);
  KJ_IF_SOME(tasks, event_processor_task_) {
    tasks->add(process_events());
    tasks->add(read_engine_events());
  }

  return kj::READY_NOW;
}

void EngineBridge::stop() {
  // Only perform cleanup if we were running
  bool was_running = running_.exchange(false, std::memory_order_acq_rel);

  if (was_running) {
    // Kill engine subprocess if running
    KJ_IF_SOME(subprocess, engine_subprocess_) {
      subprocess->kill();
    }

    event_processor_task_ = kj::none;
    engine_subprocess_ = kj::none;
    // Note: Don't clear io_context_ here - it's set during initialize() and
    // should persist across stop/start cycles. The bridge can be restarted.
  }
  // If not running, stop() is a no-op (idempotent)
}

// ============================================================================
// Order Operations
// ============================================================================

kj::Promise<void> EngineBridge::place_order(kj::StringPtr side, kj::StringPtr symbol, double qty,
                                            double price, kj::StringPtr client_order_id) {
  KJ_REQUIRE(running_.load(std::memory_order_acquire), "Engine not running");
  KJ_REQUIRE(symbol.size() > 0, "Symbol must not be empty");
  KJ_REQUIRE(qty > 0.0, "Order quantity must be positive");
  KJ_REQUIRE(client_order_id.size() > 0, "Client order ID must not be empty");
  KJ_REQUIRE(price >= 0.0, "Price must be non-negative");
  (void)parse_order_side(side);

  KJ_IF_SOME(subprocess, engine_subprocess_) {
    // Format ORDER command
    kj::String command = format_order_command(side, symbol, qty, price, client_order_id);

    auto start_ns = now_ns();
    // Write to engine stdin
    auto& stdin_stream = subprocess->stdin();
    co_await stdin_stream.write(command.asBytes());
    auto end_ns = now_ns();
    auto latency_ns = static_cast<uint64_t>(end_ns - start_ns);

    // Update metrics
    auto count = metrics_.orders_submitted.fetch_add(1, std::memory_order_relaxed) + 1;
    auto prev_avg = metrics_.avg_order_latency_ns.load(std::memory_order_relaxed);
    auto new_avg = (prev_avg * (count - 1) + latency_ns) / count;
    metrics_.avg_order_latency_ns.store(new_avg, std::memory_order_relaxed);

    auto prev_max = metrics_.max_order_latency_ns.load(std::memory_order_relaxed);
    if (latency_ns > prev_max) {
      metrics_.max_order_latency_ns.store(latency_ns, std::memory_order_relaxed);
    }
  }
  else {
    KJ_FAIL_REQUIRE("Engine subprocess not available");
  }
}

kj::Promise<void> EngineBridge::cancel_order(kj::StringPtr client_order_id) {
  KJ_REQUIRE(running_.load(std::memory_order_acquire), "Engine not running");
  KJ_REQUIRE(client_order_id.size() > 0, "Client order ID must not be empty");

  KJ_IF_SOME(subprocess, engine_subprocess_) {
    // Format CANCEL command
    kj::String command = format_cancel_command(client_order_id);

    // Write to engine stdin
    auto& stdin_stream = subprocess->stdin();
    co_await stdin_stream.write(command.asBytes());

    // Update metrics
    metrics_.orders_cancelled.fetch_add(1, std::memory_order_relaxed);
  }
  else {
    KJ_FAIL_REQUIRE("Engine subprocess not available");
  }
}

// ============================================================================
// State Queries
// ============================================================================

kj::Maybe<veloz::oms::OrderState> EngineBridge::get_order(kj::StringPtr client_order_id) const {
  auto lock = cached_state_.lockShared();
  metrics_.order_queries.fetch_add(1, std::memory_order_relaxed);

  KJ_IF_SOME(order, lock->order_states.find(client_order_id)) {
    return clone_order_state(order);
  }

  return kj::none;
}

kj::Vector<veloz::oms::OrderState> EngineBridge::get_orders() const {
  auto lock = cached_state_.lockShared();

  kj::Vector<veloz::oms::OrderState> result;
  for (const auto& entry : lock->order_states) {
    result.add(clone_order_state(entry.value));
  }

  return result;
}

kj::Vector<veloz::oms::OrderState> EngineBridge::get_pending_orders() const {
  auto lock = cached_state_.lockShared();

  kj::Vector<veloz::oms::OrderState> result;
  for (const auto& entry : lock->order_states) {
    if (!is_terminal_status(entry.value.status)) {
      result.add(clone_order_state(entry.value));
    }
  }

  return result;
}

MarketSnapshot EngineBridge::get_market_snapshot(kj::StringPtr symbol) {
  auto lock = cached_state_.lockShared();
  metrics_.market_snapshots.fetch_add(1, std::memory_order_relaxed);

  KJ_IF_SOME(snapshot, lock->market_snapshots.find(symbol)) {
    return clone_market_snapshot(snapshot);
  }

  return default_market_snapshot(symbol);
}

kj::Vector<MarketSnapshot>
EngineBridge::get_market_snapshots(kj::ArrayPtr<const kj::String> symbols) {
  kj::Vector<MarketSnapshot> result;
  result.reserve(symbols.size());
  for (const auto& symbol : symbols) {
    result.add(get_market_snapshot(symbol));
  }
  return result;
}

AccountState EngineBridge::get_account_state() {
  auto lock = cached_state_.lockShared();
  if (lock->account_state.last_update_ns == 0) {
    return default_account_state();
  }
  return clone_account_state(lock->account_state);
}

kj::Vector<veloz::oms::PositionSnapshot> EngineBridge::get_positions() {
  return kj::Vector<veloz::oms::PositionSnapshot>();
}

kj::Maybe<veloz::oms::PositionSnapshot> EngineBridge::get_position(kj::StringPtr symbol) {
  (void)symbol;
  return kj::none;
}

// ============================================================================
// Event Subscriptions
// ============================================================================

uint64_t EngineBridge::subscribe_to_events(EventCallback callback) {
  auto lock = state_.lockExclusive();

  KJ_REQUIRE(lock->subscriptions.size() < config_.max_subscriptions, "Max subscriptions reached");
  uint64_t id = lock->next_subscription_id++;
  lock->subscriptions.insert(id, kj::mv(callback));

  metrics_.active_subscriptions.fetch_add(1, std::memory_order_relaxed);

  return id;
}

uint64_t EngineBridge::subscribe_to_events(BridgeEventType filter, EventCallback callback) {
  auto lock = state_.lockExclusive();

  KJ_REQUIRE(lock->subscriptions.size() < config_.max_subscriptions, "Max subscriptions reached");
  uint64_t id = lock->next_subscription_id++;
  lock->subscriptions.insert(id, kj::mv(callback));
  lock->subscription_filters.insert(id, filter);

  metrics_.active_subscriptions.fetch_add(1, std::memory_order_relaxed);

  return id;
}

void EngineBridge::unsubscribe(uint64_t subscription_id) {
  auto lock = state_.lockExclusive();

  bool removed = lock->subscriptions.erase(subscription_id);
  lock->subscription_filters.erase(subscription_id);

  if (removed) {
    metrics_.active_subscriptions.fetch_sub(1, std::memory_order_relaxed);
  }
}

void EngineBridge::unsubscribe_all() {
  auto lock = state_.lockExclusive();

  size_t count = lock->subscriptions.size();
  lock->subscriptions.clear();
  lock->subscription_filters.clear();
  if (count > 0) {
    metrics_.active_subscriptions.fetch_sub(count, std::memory_order_relaxed);
  }
}

// ============================================================================
// Metrics and Statistics
// ============================================================================

EngineBridge::QueueStats EngineBridge::get_queue_stats() const {
  return QueueStats{.queued_events = event_queue_.size(),
                    .pool_allocated = event_queue_.pool_allocated_count(),
                    .pool_total_allocations = event_queue_.pool_total_allocations()};
}

// ============================================================================
// Private Methods
// ============================================================================

kj::Promise<void> EngineBridge::process_events() {
  // Event processing loop - runs until stopped
  // Use KJ's afterDelay with promise chaining
  if (!running_.load(std::memory_order_acquire)) {
    return kj::READY_NOW;
  }

  // Try to pop an event from the queue
  auto maybe_event = event_queue_.pop();

  KJ_IF_SOME(event, maybe_event) {
    // Publish to all subscribers
    publish_event(event);
  }

  // Schedule next iteration
  return io_context_->provider->getTimer().afterDelay(10 * kj::MICROSECONDS).then([this]() {
    return process_events();
  });
}

void EngineBridge::publish_event(const BridgeEvent& event) {
  auto lock = state_.lockExclusive();

  // Iterate through all subscriptions
  for (auto& entry : lock->subscriptions) {
    uint64_t id = entry.key;
    EventCallback& callback = entry.value;

    // Check if this subscription has a filter
    KJ_IF_SOME(filter_type, lock->subscription_filters.find(id)) {
      // Has filter - only call if event type matches
      if (event.type != filter_type) {
        continue; // Skip - event type doesn't match filter
      }
    }

    // Call callback with event
    callback(event);

    // Update delivery metric
    metrics_.events_delivered.fetch_add(1, std::memory_order_relaxed);
  }
}

veloz::exec::OrderSide EngineBridge::parse_order_side(kj::StringPtr side) {
  if (side == "buy" || side == "BUY") {
    return veloz::exec::OrderSide::Buy;
  } else if (side == "sell" || side == "SELL") {
    return veloz::exec::OrderSide::Sell;
  }

  KJ_FAIL_REQUIRE("Invalid order side", side);
}

veloz::exec::OrderType EngineBridge::parse_order_type(kj::StringPtr type) {
  if (type == "market" || type == "MARKET") {
    return veloz::exec::OrderType::Market;
  } else if (type == "limit" || type == "LIMIT") {
    return veloz::exec::OrderType::Limit;
  } else if (type == "stop_loss" || type == "STOP_LOSS") {
    return veloz::exec::OrderType::StopLoss;
  } else if (type == "stop_loss_limit" || type == "STOP_LOSS_LIMIT") {
    return veloz::exec::OrderType::StopLossLimit;
  } else if (type == "take_profit" || type == "TAKE_PROFIT") {
    return veloz::exec::OrderType::TakeProfit;
  } else if (type == "take_profit_limit" || type == "TAKE_PROFIT_LIMIT") {
    return veloz::exec::OrderType::TakeProfitLimit;
  }

  KJ_FAIL_REQUIRE("Invalid order type", type);
}

// ============================================================================
// Subprocess Communication Helpers
// ============================================================================

// Format ORDER command for engine stdin
kj::String format_order_command(kj::StringPtr side, kj::StringPtr symbol, double qty, double price,
                                kj::StringPtr client_id) {
  return kj::str("ORDER ", side, " ", symbol, " ", qty, " ", price, " ", client_id, "\n");
}

// Format CANCEL command for engine stdin
kj::String format_cancel_command(kj::StringPtr client_id) {
  return kj::str("CANCEL ", client_id, "\n");
}

// Add read_engine_events() implementation
kj::Promise<void> EngineBridge::read_engine_events() {
  if (!running_.load(std::memory_order_acquire)) {
    return kj::READY_NOW;
  }

  KJ_IF_SOME(subprocess, engine_subprocess_) {
    auto& stdout_stream = subprocess->stdout();

    // Buffer for reading
    kj::Vector<char> buffer;
    buffer.resize(4096);

    return stdout_stream.tryRead(buffer.begin(), 1, buffer.size())
        .then([this, buffer = kj::mv(buffer)](size_t n) mutable -> kj::Promise<void> {
          if (n == 0) {
            // EOF - engine process exited
            running_.store(false, std::memory_order_release);
            return kj::READY_NOW;
          }

          // Note: We use heapString to ensure NUL-termination for StringPtr operations
          kj::String chunk = kj::heapString(buffer.begin(), n);
          kj::String combined =
              pending_stdout_.size() == 0 ? kj::mv(chunk) : kj::str(pending_stdout_, chunk);
          pending_stdout_ = kj::str("");

          auto combined_ptr = combined.asPtr();
          size_t line_start = 0;
          for (size_t i = 0; i < combined.size(); ++i) {
            if (combined_ptr[i] != '\n') {
              continue;
            }

            auto line = kj::heapString(combined_ptr.begin() + line_start, i - line_start);
            line_start = i + 1;

            auto maybe_event = parse_ndjson_event(line);

            KJ_IF_SOME(event, maybe_event) {
              update_cached_state(event);
              event_queue_.push(kj::mv(event));
              metrics_.events_published.fetch_add(1, std::memory_order_relaxed);
              last_event_ns_.store(now_ns(), std::memory_order_release);
            }
          }

          if (line_start < combined.size()) {
            pending_stdout_ = kj::str(combined_ptr.slice(line_start, combined.size()));
          }

          return read_engine_events();
        });
  }
  else {
    return kj::READY_NOW;
  }
}

// Parse NDJSON event from engine stdout
kj::Maybe<BridgeEvent> parse_ndjson_event(kj::StringPtr line) {
  if (line.size() == 0) {
    return kj::none;
  }

  // Skip lines that don't start with '{' (not JSON - e.g., engine banner text)
  if (line[0] != '{') {
    return kj::none;
  }

  // Parse JSON line
  veloz::core::JsonDocument doc;
  veloz::core::JsonValue root;

  // Wrap JSON parsing in exception handling to skip malformed lines
  KJ_IF_SOME(_, kj::runCatchingExceptions([&]() {
               doc = veloz::core::JsonDocument::parse(line);
               root = doc.root();
             })) {
    // JSON parse error - skip this line
    KJ_LOG(DBG, "Skipping non-JSON line from engine", line);
    return kj::none;
  }

  if (!root.is_valid()) {
    return kj::none;
  }

  // Get event type
  auto type_val = root.get("type");
  KJ_IF_SOME(type, type_val) {
    auto type_str = type.get_string();

    // Parse based on type
    if (type_str == "order_update" || type_str == "order_state" || type_str == "order_received") {
      BridgeEvent event;
      event.type = BridgeEventType::OrderUpdate;

      // Get timestamp
      KJ_IF_SOME(ts, root.get("ts_ns")) {
        auto maybe_ts = ts.parse_as<int64_t>();
        KJ_IF_SOME(val, maybe_ts) {
          event.timestamp_ns = val;
        }
        else {
          event.timestamp_ns = now_ns();
        }
      }
      else {
        event.timestamp_ns = now_ns();
      }

      BridgeEvent::OrderUpdateData data;
      veloz::oms::OrderState state;

      // Get client_order_id
      KJ_IF_SOME(cid, root.get("client_order_id")) {
        auto maybe_str = cid.parse_as<kj::String>();
        KJ_IF_SOME(str, maybe_str) {
          state.client_order_id = kj::mv(str);
        }
      }

      // Get status
      KJ_IF_SOME(st, root.get("status")) {
        auto maybe_str = st.parse_as<kj::String>();
        KJ_IF_SOME(str, maybe_str) {
          state.status = kj::mv(str);
        }
      }

      // Get symbol
      KJ_IF_SOME(sym, root.get("symbol")) {
        auto maybe_str = sym.parse_as<kj::String>();
        KJ_IF_SOME(str, maybe_str) {
          state.symbol = kj::mv(str);
        }
      }

      // Get side
      KJ_IF_SOME(sd, root.get("side")) {
        auto maybe_str = sd.parse_as<kj::String>();
        KJ_IF_SOME(str, maybe_str) {
          state.side = kj::mv(str);
        }
      }

      // Get qty
      KJ_IF_SOME(q, root.get("qty")) {
        state.order_qty = q.parse_as_or<double>(0.0);
      }

      // Get price
      KJ_IF_SOME(p, root.get("price")) {
        KJ_IF_SOME(val, p.parse_as<double>()) {
          state.limit_price = val;
        }
      }

      // Get executed_qty
      KJ_IF_SOME(eq, root.get("executed_qty")) {
        state.executed_qty = eq.parse_as_or<double>(0.0);
      }

      // Get avg_price
      KJ_IF_SOME(ap, root.get("avg_price")) {
        state.avg_price = ap.parse_as_or<double>(0.0);
      }

      // Get venue_order_id
      KJ_IF_SOME(vo, root.get("venue_order_id")) {
        auto maybe_str = vo.parse_as<kj::String>();
        KJ_IF_SOME(str, maybe_str) {
          state.venue_order_id = kj::mv(str);
        }
      }

      // Get reason
      KJ_IF_SOME(r, root.get("reason")) {
        auto maybe_str = r.parse_as<kj::String>();
        KJ_IF_SOME(str, maybe_str) {
          state.reason = kj::mv(str);
        }
      }

      state.last_ts_ns = event.timestamp_ns;
      state.created_ts_ns = event.timestamp_ns;

      data.order_state = kj::mv(state);
      event.data.init<BridgeEvent::OrderUpdateData>(kj::mv(data));

      return kj::mv(event);
    } else if (type_str == "fill") {
      BridgeEvent event;
      event.type = BridgeEventType::Fill;

      KJ_IF_SOME(ts, root.get("ts_ns")) {
        event.timestamp_ns = ts.parse_as_or<int64_t>(now_ns());
      }
      else {
        event.timestamp_ns = now_ns();
      }

      BridgeEvent::FillData data;

      KJ_IF_SOME(cid, root.get("client_order_id")) {
        data.client_order_id = cid.parse_as_or<kj::String>(kj::str(""));
      }

      KJ_IF_SOME(sym, root.get("symbol")) {
        data.symbol = sym.parse_as_or<kj::String>(kj::str(""));
      }

      KJ_IF_SOME(q, root.get("qty")) {
        data.qty = q.parse_as_or<double>(0.0);
      }

      KJ_IF_SOME(p, root.get("price")) {
        data.price = p.parse_as_or<double>(0.0);
      }

      data.ts_ns = event.timestamp_ns;

      event.data.init<BridgeEvent::FillData>(kj::mv(data));

      return kj::mv(event);
    } else if (type_str == "market" || type_str == "trade" || type_str == "book_top") {
      BridgeEvent event;
      event.type = BridgeEventType::MarketData;

      KJ_IF_SOME(ts, root.get("ts_ns")) {
        event.timestamp_ns = ts.parse_as_or<int64_t>(now_ns());
      }
      else {
        event.timestamp_ns = now_ns();
      }

      BridgeEvent::MarketDataData data;

      KJ_IF_SOME(sym, root.get("symbol")) {
        data.symbol = sym.parse_as_or<kj::String>(kj::str(""));
      }

      KJ_IF_SOME(p, root.get("price")) {
        data.price = p.parse_as_or<double>(0.0);
      }

      data.ts_ns = event.timestamp_ns;

      event.data.init<BridgeEvent::MarketDataData>(kj::mv(data));

      return kj::mv(event);
    } else if (type_str == "account") {
      BridgeEvent event;
      event.type = BridgeEventType::Account;

      KJ_IF_SOME(ts, root.get("ts_ns")) {
        event.timestamp_ns = ts.parse_as_or<int64_t>(now_ns());
      }
      else {
        event.timestamp_ns = now_ns();
      }

      BridgeEvent::AccountData data;
      data.ts_ns = event.timestamp_ns;

      // Parse balances array
      auto balances_val = root.get("balances");
      KJ_IF_SOME(balances, balances_val) {
        balances.for_each_array([&data](const veloz::core::JsonValue& balance) {
          KJ_IF_SOME(asset, balance.get("asset")) {
            auto asset_str = asset.parse_as_or<kj::String>(kj::str(""));
            data.balances.add(kj::mv(asset_str));
          }
        });
      }

      event.data.init<BridgeEvent::AccountData>(kj::mv(data));

      return kj::mv(event);
    } else if (type_str == "error") {
      BridgeEvent event;
      event.type = BridgeEventType::Error;

      KJ_IF_SOME(ts, root.get("ts_ns")) {
        event.timestamp_ns = ts.parse_as_or<int64_t>(now_ns());
      }
      else {
        event.timestamp_ns = now_ns();
      }

      BridgeEvent::ErrorData data;

      KJ_IF_SOME(msg, root.get("message")) {
        data.message = msg.parse_as_or<kj::String>(kj::str("Unknown error"));
      }
      else {
        data.message = kj::str("Unknown error");
      }

      data.ts_ns = event.timestamp_ns;

      event.data.init<BridgeEvent::ErrorData>(kj::mv(data));

      return kj::mv(event);
    }
  }

  return kj::none;
}

void EngineBridge::update_cached_state(const BridgeEvent& event) {
  auto lock = cached_state_.lockExclusive();

  switch (event.type) {
  case BridgeEventType::OrderUpdate: {
    if (event.data.is<BridgeEvent::OrderUpdateData>()) {
      const auto& data = event.data.get<BridgeEvent::OrderUpdateData>();
      const auto& order_state = data.order_state;

      // Update or insert order in cache
      auto key = kj::str(order_state.client_order_id);
      lock->order_states.upsert(kj::mv(key), clone_order_state(order_state));
    }
    break;
  }
  case BridgeEventType::MarketData: {
    if (event.data.is<BridgeEvent::MarketDataData>()) {
      const auto& data = event.data.get<BridgeEvent::MarketDataData>();
      KJ_IF_SOME(existing, lock->market_snapshots.findEntry(data.symbol)) {
        existing.value.symbol = kj::str(data.symbol);
        existing.value.last_price = data.price;
        existing.value.last_update_ns = event.timestamp_ns;
        existing.value.exchange_ts_ns = data.ts_ns;
      }
      else {
        MarketSnapshot snapshot;
        snapshot.symbol = kj::str(data.symbol);
        snapshot.last_price = data.price;
        snapshot.last_update_ns = event.timestamp_ns;
        snapshot.exchange_ts_ns = data.ts_ns;
        auto key = kj::str(data.symbol);
        lock->market_snapshots.insert(kj::mv(key), kj::mv(snapshot));
      }
    }
    break;
  }
  case BridgeEventType::Account: {
    if (event.data.is<BridgeEvent::AccountData>()) {
      const auto& data = event.data.get<BridgeEvent::AccountData>();
      AccountState state;
      state.last_update_ns = data.ts_ns;
      for (const auto& balance : data.balances) {
        state.balances.insert(kj::str(balance), 0.0);
      }
      lock->account_state = kj::mv(state);
    }
    break;
  }
  default:
    // No state update needed for other event types
    break;
  }
}

} // namespace veloz::gateway::bridge
