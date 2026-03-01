#pragma once

#include "veloz/backtest/types.h"
#include "veloz/market/market_event.h"
#include "veloz/strategy/strategy.h"

// KJ library includes
#include <atomic>
#include <cstdint>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/refcount.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::backtest {

// Forward declarations
class IDataSource;
class IBacktestEngine;

/**
 * @brief Event priority for backtest event queue
 *
 * Higher priority events are processed before lower priority ones.
 */
enum class BacktestEventPriority : uint8_t {
  Low = 0,     ///< Low priority (cleanup, logging)
  Normal = 1,  ///< Normal priority (market data)
  High = 2,    ///< High priority (order fills)
  Critical = 3 ///< Critical priority (risk events, stop loss)
};

/**
 * @brief Backtest event types
 */
enum class BacktestEventType : uint8_t {
  MarketData = 0, ///< Market data event (trade, kline, book)
  OrderFill = 1,  ///< Order fill event
  Timer = 2,      ///< Timer event
  RiskCheck = 3,  ///< Risk check event
  Custom = 4      ///< Custom event
};

/**
 * @brief Backtest event wrapper
 *
 * Wraps market events with additional backtest-specific metadata
 * for priority-based event processing.
 */
struct BacktestEvent {
  BacktestEventType type{BacktestEventType::MarketData};
  BacktestEventPriority priority{BacktestEventPriority::Normal};
  std::int64_t timestamp_ns{0};            ///< Event timestamp in nanoseconds
  std::int64_t sequence{0};                ///< Sequence number for ordering
  veloz::market::MarketEvent market_event; ///< Underlying market event
  kj::String custom_data;                  ///< Custom event data (JSON)

  // Comparison for priority queue (higher priority first, then earlier timestamp)
  bool operator>(const BacktestEvent& other) const {
    if (priority != other.priority) {
      return static_cast<uint8_t>(priority) > static_cast<uint8_t>(other.priority);
    }
    if (timestamp_ns != other.timestamp_ns) {
      return timestamp_ns < other.timestamp_ns; // Earlier events first
    }
    return sequence < other.sequence; // Lower sequence first
  }
};

/**
 * @brief Virtual clock for backtest time simulation
 *
 * Provides simulated time that advances based on events rather than
 * wall clock time. Supports pause/resume and time scaling.
 */
class VirtualClock {
public:
  VirtualClock();
  ~VirtualClock() = default;

  // Non-copyable, non-movable (due to std::atomic member)
  VirtualClock(const VirtualClock&) = delete;
  VirtualClock& operator=(const VirtualClock&) = delete;
  VirtualClock(VirtualClock&&) = delete;
  VirtualClock& operator=(VirtualClock&&) = delete;

  /**
   * @brief Set the start time of the simulation
   * @param start_time_ns Start time in nanoseconds since epoch
   */
  void set_start_time(std::int64_t start_time_ns);

  /**
   * @brief Set the end time of the simulation
   * @param end_time_ns End time in nanoseconds since epoch
   */
  void set_end_time(std::int64_t end_time_ns);

  /**
   * @brief Advance the clock to a specific time
   * @param time_ns Time in nanoseconds since epoch
   * @return true if time advanced, false if time would go backwards
   */
  bool advance_to(std::int64_t time_ns);

  /**
   * @brief Get the current simulated time
   * @return Current time in nanoseconds since epoch
   */
  [[nodiscard]] std::int64_t now_ns() const;

  /**
   * @brief Get the current simulated time in milliseconds
   * @return Current time in milliseconds since epoch
   */
  [[nodiscard]] std::int64_t now_ms() const;

  /**
   * @brief Get the start time
   * @return Start time in nanoseconds since epoch
   */
  [[nodiscard]] std::int64_t start_time_ns() const;

  /**
   * @brief Get the end time
   * @return End time in nanoseconds since epoch
   */
  [[nodiscard]] std::int64_t end_time_ns() const;

  /**
   * @brief Get progress as a fraction (0.0 to 1.0)
   * @return Progress fraction
   */
  [[nodiscard]] double progress() const;

  /**
   * @brief Get elapsed time since start
   * @return Elapsed time in nanoseconds
   */
  [[nodiscard]] std::int64_t elapsed_ns() const;

  /**
   * @brief Get remaining time until end
   * @return Remaining time in nanoseconds
   */
  [[nodiscard]] std::int64_t remaining_ns() const;

  /**
   * @brief Reset the clock to start time
   */
  void reset();

private:
  std::atomic<std::int64_t> current_time_ns_{0};
  std::int64_t start_time_ns_{0};
  std::int64_t end_time_ns_{0};
};

/**
 * @brief Backtest engine state
 */
enum class BacktestState : uint8_t {
  Idle = 0,    ///< Not initialized or reset
  Initialized, ///< Initialized but not running
  Running,     ///< Currently running
  Paused,      ///< Paused (can be resumed)
  Completed,   ///< Completed successfully
  Stopped,     ///< Stopped by user
  Error        ///< Error occurred
};

/**
 * @brief Progress information for callbacks
 */
struct BacktestProgress {
  double progress_fraction{0.0};    ///< Progress as fraction (0.0 to 1.0)
  std::int64_t events_processed{0}; ///< Number of events processed
  std::int64_t total_events{0};     ///< Total number of events
  std::int64_t current_time_ns{0};  ///< Current simulated time
  std::int64_t elapsed_real_ns{0};  ///< Real elapsed time
  double events_per_second{0.0};    ///< Processing rate
  BacktestState state{BacktestState::Idle};
  kj::String message; ///< Optional status message
};

/**
 * @brief Data source interface
 *
 * Inherits from kj::Refcounted to support kj::Rc<IDataSource> reference counting.
 * Note: kj::Rc is single-threaded; use kj::Arc for thread-safe sharing.
 */
class IDataSource : public kj::Refcounted {
public:
  ~IDataSource() noexcept(false) override = default;

  virtual bool connect() = 0;
  virtual bool disconnect() = 0;

  virtual kj::Vector<veloz::market::MarketEvent>
  get_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
           kj::StringPtr data_type, kj::StringPtr time_frame) = 0;

  virtual bool download_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
                             kj::StringPtr data_type, kj::StringPtr time_frame,
                             kj::StringPtr output_path) = 0;
};

// Backtest engine interface
class IBacktestEngine {
public:
  virtual ~IBacktestEngine() = default;

  virtual bool initialize(const BacktestConfig& config) = 0;
  virtual bool run() = 0;
  virtual bool stop() = 0;
  virtual bool reset() = 0;

  /**
   * @brief Pause the backtest (can be resumed)
   * @return true if paused successfully
   */
  virtual bool pause() = 0;

  /**
   * @brief Resume a paused backtest
   * @return true if resumed successfully
   */
  virtual bool resume() = 0;

  /**
   * @brief Get the current backtest state
   * @return Current state
   */
  virtual BacktestState get_state() const = 0;

  /**
   * @brief Get the virtual clock for time queries
   * @return Reference to virtual clock
   */
  virtual const VirtualClock& get_clock() const = 0;

  virtual BacktestResult get_result() const = 0;
  // kj::Rc: matches strategy module's API (IStrategyFactory returns kj::Rc)
  virtual void set_strategy(kj::Rc<veloz::strategy::IStrategy> strategy) = 0;
  // kj::Rc: matches DataSourceFactory return type for reference-counted ownership
  virtual void set_data_source(kj::Rc<IDataSource> data_source) = 0;

  /**
   * @brief Set progress callback (legacy - simple progress fraction)
   */
  virtual void on_progress(kj::Function<void(double progress)> callback) = 0;

  /**
   * @brief Set detailed progress callback
   * @param callback Function called with detailed progress information
   */
  virtual void on_progress_detailed(kj::Function<void(const BacktestProgress&)> callback) = 0;

  /**
   * @brief Set state change callback
   * @param callback Function called when state changes
   */
  virtual void on_state_change(kj::Function<void(BacktestState, BacktestState)> callback) = 0;
};

// Backtest engine implementation
class BacktestEngine : public IBacktestEngine {
public:
  BacktestEngine();
  ~BacktestEngine() noexcept override;

  bool initialize(const BacktestConfig& config) override;
  bool run() override;
  bool stop() override;
  bool reset() override;
  bool pause() override;
  bool resume() override;

  BacktestState get_state() const override;
  const VirtualClock& get_clock() const override;

  BacktestResult get_result() const override;
  void set_strategy(kj::Rc<veloz::strategy::IStrategy> strategy) override;
  void set_data_source(kj::Rc<IDataSource> data_source) override;

  void on_progress(kj::Function<void(double progress)> callback) override;
  void on_progress_detailed(kj::Function<void(const BacktestProgress&)> callback) override;
  void on_state_change(kj::Function<void(BacktestState, BacktestState)> callback) override;

  // Additional methods for event-driven backtest

  /**
   * @brief Add a custom event to the event queue
   * @param event The event to add
   */
  void add_event(BacktestEvent event);

  /**
   * @brief Get the number of pending events in the queue
   * @return Number of pending events
   */
  [[nodiscard]] size_t pending_events() const;

  /**
   * @brief Process a single event from the queue
   * @return true if an event was processed, false if queue is empty
   */
  bool process_single_event();

  /**
   * @brief Step through the backtest one event at a time (for debugging)
   * @return true if an event was processed
   */
  bool step();

private:
  struct Impl;
  kj::Own<Impl> impl_;

  // Internal methods
  void transition_state(BacktestState new_state);
  void report_progress();
  bool process_event(BacktestEvent& event);
  void load_events_from_data_source();
};

/**
 * @brief Convert BacktestState to string
 */
[[nodiscard]] kj::StringPtr to_string(BacktestState state);

/**
 * @brief Convert BacktestEventPriority to string
 */
[[nodiscard]] kj::StringPtr to_string(BacktestEventPriority priority);

/**
 * @brief Convert BacktestEventType to string
 */
[[nodiscard]] kj::StringPtr to_string(BacktestEventType type);

} // namespace veloz::backtest
