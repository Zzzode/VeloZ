#pragma once

// Note: For initial implementation, EngineBridge operates independently
// When full integration with veloz::engine is needed, include:
// #include "veloz/engine/engine_app.h"
// And update CMakeLists.txt to link to: veloz_engine veloz_market veloz_exec veloz_oms
// veloz_strategy

#include "veloz/core/lockfree_queue.h"
#include "veloz/market/market_event.h"
#include "veloz/oms/order_record.h"
#include "veloz/oms/position.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway::bridge {

// Forward declaration
class SubprocessHandle;

// ============================================================================
// Event Types
// ============================================================================

/**
 * @brief Bridge event type for internal event routing
 */
enum class BridgeEventType : std::uint8_t {
  OrderUpdate = 0, ///< Order status update
  Fill = 1,        ///< Trade fill
  MarketData = 2,  ///< Market data update
  Account = 3,     ///< Account/balance update
  Strategy = 4,    ///< Strategy event
  Error = 5,       ///< Error event
  EngineState = 6, ///< Engine lifecycle state
};

/**
 * @brief Bridge event wrapper for all engine events
 */
struct BridgeEvent {
  BridgeEventType type;
  std::int64_t timestamp_ns;

  // Event data (using kj::OneOf for type-safe union)
  // Note: kj::OneOf requires explicit variant types
  struct OrderUpdateData {
    veloz::oms::OrderState order_state;
  };
  struct FillData {
    kj::String client_order_id;
    kj::String symbol;
    double qty;
    double price;
    std::int64_t ts_ns;
  };
  struct MarketDataData {
    kj::String symbol;
    double price;
    std::int64_t ts_ns;
  };
  struct AccountData {
    std::int64_t ts_ns;
    kj::Vector<kj::String> balances;
  };
  struct ErrorData {
    kj::String message;
    std::int64_t ts_ns;
  };

  // Event type tag for KJ OneOf (replaces std::monostate)
  struct EmptyData {
    bool operator==(const EmptyData&) const {
      return true;
    }
  };

  kj::OneOf<EmptyData, OrderUpdateData, FillData, MarketDataData, AccountData, ErrorData> data;
};

// ============================================================================
// Market Snapshot
// ============================================================================

/**
 * @brief Snapshot of current market state for a symbol
 */
struct MarketSnapshot {
  kj::String symbol;

  // Best bid/ask
  kj::Maybe<double> best_bid_price;
  kj::Maybe<double> best_bid_qty;
  kj::Maybe<double> best_ask_price;
  kj::Maybe<double> best_ask_qty;

  // Market stats
  double last_price{0.0};
  double volume_24h{0.0};
  std::int64_t last_trade_id{0};

  // Timestamps
  std::int64_t last_update_ns{0};
  std::int64_t exchange_ts_ns{0};
};

/**
 * @brief Complete account state snapshot
 */
struct AccountState {
  // Total equity
  double total_equity{0.0};
  double available_balance{0.0};
  double unrealized_pnl{0.0};

  // Per-asset balances (symbol -> amount)
  kj::HashMap<kj::String, double> balances;

  // Position summary
  size_t open_position_count{0};
  double total_position_notional{0.0};

  // Timestamp
  std::int64_t last_update_ns{0};
};

// ============================================================================
// Engine Configuration
// ============================================================================

/**
 * @brief Configuration for EngineBridge
 */
struct EngineBridgeConfig {
  size_t event_queue_capacity{10000};       ///< Max events in queue
  bool enable_metrics{true};                ///< Enable performance metrics
  uint32_t max_subscriptions{1000};         ///< Max event subscriptions
  kj::Maybe<kj::String> engine_binary_path; ///< Path to engine binary

  static EngineBridgeConfig withDefaults() {
    return EngineBridgeConfig{};
  }
};

// ============================================================================
// Performance Metrics
// ============================================================================

/**
 * @brief Performance metrics for EngineBridge operations
 */
struct BridgeMetrics {
  std::atomic<uint64_t> orders_submitted{0};
  std::atomic<uint64_t> orders_cancelled{0};
  std::atomic<uint64_t> events_published{0};
  std::atomic<uint64_t> events_delivered{0};
  std::atomic<uint64_t> market_snapshots{0};
  std::atomic<uint64_t> order_queries{0};
  std::atomic<uint64_t> active_subscriptions{0};

  // Timing metrics (nanoseconds)
  std::atomic<uint64_t> avg_order_latency_ns{0};
  std::atomic<uint64_t> max_order_latency_ns{0};

  void reset() {
    orders_submitted.store(0, std::memory_order_relaxed);
    orders_cancelled.store(0, std::memory_order_relaxed);
    events_published.store(0, std::memory_order_relaxed);
    events_delivered.store(0, std::memory_order_relaxed);
    market_snapshots.store(0, std::memory_order_relaxed);
    order_queries.store(0, std::memory_order_relaxed);
    active_subscriptions.store(0, std::memory_order_relaxed);
    avg_order_latency_ns.store(0, std::memory_order_relaxed);
    max_order_latency_ns.store(0, std::memory_order_relaxed);
  }
};

// ============================================================================
// Engine Bridge
// ============================================================================

/**
 * @brief Bridge between C++ Gateway and C++ Engine for single-process integration
 *
 * Provides direct method calls to the engine with zero-copy event handling
 * where possible. Uses lock-free queues for high-throughput event distribution.
 *
 * Performance target: <10μs for in-process order submission
 */
class EngineBridge final {
public:
  /**
   * @brief Event subscription callback type
   */
  using EventCallback = kj::Function<void(const BridgeEvent&)>;

  /**
   * @brief Construct EngineBridge with engine reference
   */
  explicit EngineBridge(EngineBridgeConfig config);

  /**
   * @brief Destructor
   */
  ~EngineBridge();

  // Non-copyable, non-movable
  EngineBridge(const EngineBridge&) = delete;
  EngineBridge& operator=(const EngineBridge&) = delete;
  EngineBridge(EngineBridge&&) = delete;
  EngineBridge& operator=(EngineBridge&&) = delete;

  // ============================================================================
  // Engine Lifecycle
  // ============================================================================

  /**
   * @brief Initialize the bridge and connect to engine
   */
  kj::Promise<void> initialize(kj::AsyncIoContext& io);

  /**
   * @brief Start the bridge event processing
   */
  kj::Promise<void> start();

  /**
   * @brief Stop the bridge and cleanup
   */
  void stop();

  /**
   * @brief Check if bridge is running
   */
  [[nodiscard]] bool is_running() const noexcept {
    return running_.load(std::memory_order_acquire);
  }

  // ============================================================================
  // Order Operations
  // ============================================================================

  /**
   * @brief Submit an order to the engine (async)
   *
   * @param side Order side ("buy" or "sell")
   * @param symbol Trading symbol (e.g., "BTCUSDT")
   * @param qty Order quantity
   * @param price Limit price (0.0 for market orders)
   * @param client_id Client order ID for correlation
   * @return Promise that completes when order is queued
   */
  kj::Promise<void> place_order(kj::StringPtr side, kj::StringPtr symbol, double qty, double price,
                                kj::StringPtr client_id);

  /**
   * @brief Submit a cancel order request (async)
   *
   * @param client_id Client order ID to cancel
   * @return Promise that completes when cancel is queued
   */
  kj::Promise<void> cancel_order(kj::StringPtr client_id);

  /**
   * @brief Query order status by client ID
   */
  [[nodiscard]] kj::Maybe<veloz::oms::OrderState> get_order(kj::StringPtr client_id) const;

  /**
   * @brief Get all orders
   */
  [[nodiscard]] kj::Vector<veloz::oms::OrderState> get_orders() const;

  /**
   * @brief Get all pending (non-terminal) orders
   */
  [[nodiscard]] kj::Vector<veloz::oms::OrderState> get_pending_orders() const;

  // ============================================================================
  // Market Data Access
  // ============================================================================

  /**
   * @brief Get current market snapshot for a symbol
   */
  [[nodiscard]] MarketSnapshot get_market_snapshot(kj::StringPtr symbol);

  /**
   * @brief Get multiple market snapshots
   */
  [[nodiscard]] kj::Vector<MarketSnapshot>
  get_market_snapshots(kj::ArrayPtr<const kj::String> symbols);

  // ============================================================================
  // Account State
  // ============================================================================

  /**
   * @brief Get current account state
   */
  [[nodiscard]] AccountState get_account_state();

  /**
   * @brief Get positions for all symbols
   */
  [[nodiscard]] kj::Vector<veloz::oms::PositionSnapshot> get_positions();

  /**
   * @brief Get position for a specific symbol
   */
  [[nodiscard]] kj::Maybe<veloz::oms::PositionSnapshot> get_position(kj::StringPtr symbol);

  // ============================================================================
  // Event Subscription
  // ============================================================================

  /**
   * @brief Subscribe to all bridge events
   *
   * @param callback Function to call for each event
   * @return Subscription ID for unsubscribing
   */
  [[nodiscard]] uint64_t subscribe_to_events(EventCallback callback);

  /**
   * @brief Subscribe to specific event type
   *
   * @param type Event type to filter
   * @param callback Function to call for matching events
   * @return Subscription ID for unsubscribing
   */
  [[nodiscard]] uint64_t subscribe_to_events(BridgeEventType type, EventCallback callback);

  /**
   * @brief Unsubscribe from events
   *
   * @param subscription_id Subscription ID returned from subscribe_to_events
   */
  void unsubscribe(uint64_t subscription_id);

  /**
   * @brief Unsubscribe all listeners
   */
  void unsubscribe_all();

  // ============================================================================
  // Metrics and Statistics
  // ============================================================================

  /**
   * @brief Get bridge performance metrics
   */
  [[nodiscard]] const BridgeMetrics& metrics() const noexcept {
    return metrics_;
  }

  /**
   * @brief Reset performance metrics
   */
  void reset_metrics() {
    metrics_.reset();
  }

  /**
   * @brief Get event queue statistics
   */
  struct QueueStats {
    size_t queued_events;
    size_t pool_allocated;
    size_t pool_total_allocations;
  };

  [[nodiscard]] QueueStats get_queue_stats() const;

private:
  class TaskErrorHandler final : public kj::TaskSet::ErrorHandler {
  public:
    void taskFailed(kj::Exception&& exception) override;
  };

  // Internal state
  struct State {
    kj::HashMap<uint64_t, EventCallback> subscriptions;
    kj::HashMap<uint64_t, BridgeEventType> subscription_filters; // Empty filter = all events
    uint64_t next_subscription_id{1};
  };

  // Event processing
  kj::Promise<void> process_events();

  // Engine subprocess communication
  kj::Promise<void> read_engine_events();
  void update_cached_state(const BridgeEvent& event);

  // Publish event to all subscribers
  void publish_event(const BridgeEvent& event);

  // Order helpers
  veloz::exec::OrderSide parse_order_side(kj::StringPtr side);
  veloz::exec::OrderType parse_order_type(kj::StringPtr type);

  // Configuration
  EngineBridgeConfig config_;

  // Event queue (lock-free for high throughput)
  veloz::core::LockFreeQueue<BridgeEvent> event_queue_;

  // Subscription state (protected by mutex)
  kj::MutexGuarded<State> state_;

  // Metrics
  mutable BridgeMetrics metrics_;

  // Runtime state
  std::atomic<bool> running_{false};
  TaskErrorHandler task_error_handler_;

  // KJ async context (set during initialization)
  kj::AsyncIoContext* io_context_{nullptr};
  kj::Maybe<kj::Own<kj::TaskSet>> event_processor_task_;

  // Subprocess handle for engine process
  kj::Maybe<kj::Own<class SubprocessHandle>> engine_subprocess_;

  // State caches (protected by mutex for thread-safe access)
  struct CachedState {
    kj::HashMap<kj::String, veloz::oms::OrderState> order_states;
    AccountState account_state;
    kj::HashMap<kj::String, MarketSnapshot> market_snapshots;
  };
  kj::MutexGuarded<CachedState> cached_state_;

  // Health monitoring
  kj::String pending_stdout_;
  std::atomic<std::int64_t> last_event_ns_{0};
};

} // namespace veloz::gateway::bridge
