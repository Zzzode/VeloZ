/**
 * @file strategy_hooks.h
 * @brief Engine-level strategy hooks for lifecycle management
 *
 * This file defines the hooks interface for strategy lifecycle events at the engine level.
 * Strategies can register callbacks for lifecycle events, and the engine can
 * trigger these events for error handling, state persistence, and recovery.
 */

#pragma once

#include "veloz/core/logger.h"
#include "veloz/exec/order_api.h"
#include "veloz/strategy/strategy.h"

#include <kj/function.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::engine {

/**
 * @brief Strategy lifecycle event types
 */
enum class StrategyLifecycleEvent {
  Loaded,       ///< Strategy has been loaded
  Started,      ///< Strategy has been started
  Stopped,      ///< Strategy has been stopped
  Paused,       ///< Strategy has been paused
  Resumed,      ///< Strategy has been resumed
  Unloaded,     ///< Strategy has been unloaded
  Error,        ///< Strategy encountered an error
  OrderSent,    ///< Strategy sent an order
  OrderFilled,  ///< Strategy's order was filled
  OrderRejected ///< Strategy's order was rejected
};

/**
 * @brief Strategy lifecycle event payload
 */
struct StrategyLifecyclePayload {
  StrategyLifecycleEvent event_type; ///< Type of event
  kj::String strategy_id;            ///< Strategy ID
  kj::String strategy_name;          ///< Strategy name
  strategy::StrategyType type;       ///< Strategy type

  // Optional event-specific data
  kj::Maybe<kj::String> error_message;                    ///< Error message (for Error events)
  kj::Maybe<const veloz::exec::PlaceOrderRequest*> order; ///< Order pointer (for order events)
  kj::Maybe<kj::String> rejection_reason; ///< Rejection reason (for OrderRejected events)

  // Disable copy (contains kj::String which is not copyable)
  KJ_DISALLOW_COPY(StrategyLifecyclePayload);

  // Enable move
  StrategyLifecyclePayload() = default;
  StrategyLifecyclePayload(StrategyLifecyclePayload&&) = default;
  StrategyLifecyclePayload& operator=(StrategyLifecyclePayload&&) = default;
};

/**
 * @brief Strategy hooks interface
 *
 * Defines callbacks that can be triggered at various points in the strategy lifecycle.
 * This allows for error handling, logging, state persistence, and custom behavior.
 */
class StrategyHooks {
public:
  using LifecycleCallback = kj::Function<void(const StrategyLifecyclePayload&)>;
  using SignalCallback = kj::Function<void(const kj::Vector<veloz::exec::PlaceOrderRequest>&)>;

  StrategyHooks() = default;
  virtual ~StrategyHooks() = default;

  /**
   * @brief Register a callback for lifecycle events
   * @param callback Function to call when lifecycle events occur
   */
  void set_lifecycle_callback(LifecycleCallback callback) {
    lifecycle_callback_ = kj::mv(callback);
  }

  /**
   * @brief Register a callback for signal generation
   * @param callback Function to call when strategies generate trading signals
   */
  void set_signal_callback(SignalCallback callback) {
    signal_callback_ = kj::mv(callback);
  }

  /**
   * @brief Trigger a lifecycle event
   * @param payload Event payload
   */
  void trigger_lifecycle_event(const StrategyLifecyclePayload& payload) {
    KJ_IF_SOME(cb, lifecycle_callback_) {
      try {
        cb(payload);
      } catch (...) {
        // Error isolation: don't let hook errors propagate
      }
    }
  }

  /**
   * @brief Trigger signal routing
   * @param signals Trading signals to route
   */
  void route_signals(const kj::Vector<veloz::exec::PlaceOrderRequest>& signals) {
    KJ_IF_SOME(cb, signal_callback_) {
      try {
        cb(signals);
      } catch (...) {
        // Error isolation: don't let hook errors propagate
      }
    }
  }

private:
  kj::Maybe<LifecycleCallback> lifecycle_callback_;
  kj::Maybe<SignalCallback> signal_callback_;
};

/**
 * @brief Default strategy hooks implementation with error handling
 *
 * Provides default behavior for strategy lifecycle events including
 * error logging, state persistence, and recovery.
 */
class DefaultStrategyHooks : public StrategyHooks {
public:
  explicit DefaultStrategyHooks(veloz::core::Logger& logger) : logger_(logger) {}
  ~DefaultStrategyHooks() override = default;

  // Default implementations can be added here for common behaviors

private:
  veloz::core::Logger& logger_;
};

/**
 * @brief Create a lifecycle event payload
 * @param event_type Type of event
 * @param strategy_id Strategy ID
 * @param strategy_name Strategy name
 * @param type Strategy type
 * @return Event payload
 */
inline StrategyLifecyclePayload make_lifecycle_payload(StrategyLifecycleEvent event_type,
                                                       kj::StringPtr strategy_id,
                                                       kj::StringPtr strategy_name,
                                                       strategy::StrategyType type) {
  StrategyLifecyclePayload payload;
  payload.event_type = event_type;
  payload.strategy_id = kj::str(strategy_id);
  payload.strategy_name = kj::str(strategy_name);
  payload.type = type;
  return kj::mv(payload);
}

/**
 * @brief Create an error lifecycle event payload
 * @param strategy_id Strategy ID
 * @param strategy_name Strategy name
 * @param type Strategy type
 * @param error_message Error message
 * @return Event payload
 */
inline StrategyLifecyclePayload make_error_payload(kj::StringPtr strategy_id,
                                                   kj::StringPtr strategy_name,
                                                   strategy::StrategyType type,
                                                   kj::StringPtr error_message) {
  auto payload =
      make_lifecycle_payload(StrategyLifecycleEvent::Error, strategy_id, strategy_name, type);
  payload.error_message = kj::str(error_message);
  return kj::mv(payload);
}

/**
 * @brief Create an order lifecycle event payload
 * @param event_type Type of order event
 * @param strategy_id Strategy ID
 * @param strategy_name Strategy name
 * @param type Strategy type
 * @param order Order request pointer
 * @return Event payload
 */
inline StrategyLifecyclePayload make_order_payload(StrategyLifecycleEvent event_type,
                                                   kj::StringPtr strategy_id,
                                                   kj::StringPtr strategy_name,
                                                   strategy::StrategyType type,
                                                   const veloz::exec::PlaceOrderRequest* order) {
  auto payload = make_lifecycle_payload(event_type, strategy_id, strategy_name, type);
  payload.order = order;
  return kj::mv(payload);
}

} // namespace veloz::engine
