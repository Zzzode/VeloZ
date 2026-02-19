#include "veloz/core/logger.h"
#include "veloz/strategy/strategy.h"

// std::chrono for wall clock timestamps (KJ time is async I/O only)
#include <chrono>
#include <iomanip>
#include <kj/debug.h>
// std::random for ID generation (KJ lacks RNG)
#include <random>
#include <sstream>

namespace veloz::strategy {

StrategyManager::StrategyManager() {
  logger_ = kj::heap<core::Logger>();
  // Initialize the thread-safe state
  auto lock = state_.lockExclusive();
  // State is default-initialized
}

StrategyManager::~StrategyManager() {
  // Stop all strategies using kj::HashMap iteration
  for (auto& entry : strategies_) {
    if (entry.value != nullptr) {
      entry.value->on_stop();
    }
  }
  strategies_.clear();
}

void StrategyManager::register_strategy_factory(kj::Rc<IStrategyFactory> factory) {
  KJ_REQUIRE(factory != nullptr, "Factory cannot be null");

  kj::StringPtr type_name = factory->get_strategy_type();
  // kj::HashMap find pattern - returns kj::Maybe
  KJ_IF_SOME(existing, factories_.find(type_name)) {
    (void)existing;
    logger_->warn(kj::str("Strategy type ", type_name, " already registered").cStr());
    return;
  }

  // Register in legacy map for backward compatibility using kj::HashMap upsert
  factories_.upsert(kj::str(type_name), factory.addRef(),
                    [](kj::Rc<IStrategyFactory>&, kj::Rc<IStrategyFactory>&&) {});

  // Also register in thread-safe state for new runtime methods
  {
    auto lock = state_.lockExclusive();
    lock->factories.upsert(kj::str(type_name), kj::mv(factory),
                           [](kj::Rc<IStrategyFactory>&, kj::Rc<IStrategyFactory>&&) {});
  }

  logger_->info(kj::str("Strategy type ", type_name, " registered successfully").cStr());
}

kj::Rc<IStrategy> StrategyManager::create_strategy(const StrategyConfig& config) {
  // Convert StrategyType to string for factory lookup
  kj::StringPtr type_name;
  switch (config.type) {
  case StrategyType::TrendFollowing:
    type_name = "TrendFollowing"_kj;
    break;
  case StrategyType::MeanReversion:
    type_name = "MeanReversion"_kj;
    break;
  case StrategyType::Momentum:
    type_name = "Momentum"_kj;
    break;
  case StrategyType::Arbitrage:
    type_name = "Arbitrage"_kj;
    break;
  case StrategyType::MarketMaking:
    type_name = "MarketMaking"_kj;
    break;
  case StrategyType::Grid:
    type_name = "Grid"_kj;
    break;
  case StrategyType::Custom:
  default:
    type_name = "Custom"_kj;
    break;
  }

  // Lookup strategy factory using kj::HashMap find pattern
  KJ_IF_SOME(factory, factories_.find(type_name)) {
    // Create strategy instance
    kj::Rc<IStrategy> strategy = factory->create_strategy(config);
    if (strategy != nullptr) {
      // Use the strategy's own ID for storage
      kj::StringPtr strategy_id = strategy->get_id();

      // Store in legacy map for backward compatibility using kj::HashMap upsert
      strategies_.upsert(kj::str(strategy_id), strategy.addRef(),
                         [](kj::Rc<IStrategy>&, kj::Rc<IStrategy>&&) {});

      // Also store in thread-safe state for new runtime methods
      {
        auto lock = state_.lockExclusive();
        lock->strategies.upsert(kj::str(strategy_id), strategy.addRef(),
                                [](kj::Rc<IStrategy>&, kj::Rc<IStrategy>&&) {});
      }

      logger_->info(kj::str("Strategy created: ", strategy_id, " (", config.name, ")").cStr());
    }
    return strategy;
  }

  logger_->error(kj::str("Strategy type not registered: ", type_name).cStr());
  return nullptr;
}

bool StrategyManager::start_strategy(kj::StringPtr strategy_id) {
  KJ_IF_SOME(strategy, strategies_.find(strategy_id)) {
    if (strategy != nullptr) {
      strategy->on_start();
      logger_->info(kj::str("Strategy started: ", strategy_id).cStr());
      return true;
    }
  }

  logger_->error(kj::str("Strategy not found: ", strategy_id).cStr());
  return false;
}

bool StrategyManager::stop_strategy(kj::StringPtr strategy_id) {
  KJ_IF_SOME(strategy, strategies_.find(strategy_id)) {
    if (strategy != nullptr) {
      strategy->on_stop();
      logger_->info(kj::str("Strategy stopped: ", strategy_id).cStr());
      return true;
    }
  }

  logger_->error(kj::str("Strategy not found: ", strategy_id).cStr());
  return false;
}

bool StrategyManager::remove_strategy(kj::StringPtr strategy_id) {
  KJ_IF_SOME(strategy, strategies_.find(strategy_id)) {
    if (strategy != nullptr) {
      strategy->on_stop();
    }
    strategies_.erase(strategy_id);

    // Also remove from thread-safe state
    {
      auto lock = state_.lockExclusive();
      lock->strategies.erase(strategy_id);
    }

    logger_->info(kj::str("Strategy removed: ", strategy_id).cStr());
    return true;
  }

  logger_->error(kj::str("Strategy not found: ", strategy_id).cStr());
  return false;
}

kj::Rc<IStrategy> StrategyManager::get_strategy(kj::StringPtr strategy_id) {
  KJ_IF_SOME(strategy, strategies_.find(strategy_id)) {
    return strategy.addRef();
  }

  logger_->error(kj::str("Strategy not found: ", strategy_id).cStr());
  return nullptr;
}

kj::Vector<StrategyState> StrategyManager::get_all_strategy_states() const {
  kj::Vector<StrategyState> states;

  // kj::HashMap iteration uses entry with .key and .value
  for (const auto& entry : strategies_) {
    if (entry.value != nullptr) {
      states.add(entry.value->get_state());
    }
  }

  return states;
}

kj::Vector<kj::String> StrategyManager::get_all_strategy_ids() const {
  kj::Vector<kj::String> ids;

  // kj::HashMap iteration uses entry with .key and .value
  for (const auto& entry : strategies_) {
    ids.add(kj::str(entry.key));
  }

  return ids;
}

void StrategyManager::on_market_event(const veloz::market::MarketEvent& event) {
  // kj::HashMap iteration uses entry with .key and .value
  for (auto& entry : strategies_) {
    if (entry.value != nullptr) {
      entry.value->on_event(event);
    }
  }
}

void StrategyManager::on_position_update(const veloz::oms::Position& position) {
  // kj::HashMap iteration uses entry with .key and .value
  for (auto& entry : strategies_) {
    if (entry.value != nullptr) {
      entry.value->on_position_update(position);
    }
  }
}

void StrategyManager::on_timer(int64_t timestamp) {
  // kj::HashMap iteration uses entry with .key and .value
  for (auto& entry : strategies_) {
    if (entry.value != nullptr) {
      entry.value->on_timer(timestamp);
    }
  }
}

kj::Vector<veloz::exec::PlaceOrderRequest> StrategyManager::get_all_signals() {
  kj::Vector<veloz::exec::PlaceOrderRequest> all_signals;

  // kj::HashMap iteration uses entry with .key and .value
  for (auto& entry : strategies_) {
    if (entry.value != nullptr) {
      auto signals = entry.value->get_signals();
      for (size_t i = 0; i < signals.size(); ++i) {
        all_signals.add(kj::mv(signals[i]));
      }
    }
  }

  return all_signals;
}

// Runtime integration methods

kj::String StrategyManager::load_strategy(const StrategyConfig& config,
                                          veloz::core::Logger& logger) {
  auto lock = state_.lockExclusive();

  // Generate unique strategy ID
  // std::random for ID generation (KJ lacks RNG)
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(100000, 999999);
  kj::String strategy_id = kj::str("strat-", dis(gen));

  // Convert StrategyType to string for factory lookup
  kj::StringPtr type_name;
  switch (config.type) {
  case StrategyType::TrendFollowing:
    type_name = "TrendFollowing"_kj;
    break;
  case StrategyType::MeanReversion:
    type_name = "MeanReversion"_kj;
    break;
  case StrategyType::Momentum:
    type_name = "Momentum"_kj;
    break;
  case StrategyType::Arbitrage:
    type_name = "Arbitrage"_kj;
    break;
  case StrategyType::MarketMaking:
    type_name = "MarketMaking"_kj;
    break;
  case StrategyType::Grid:
    type_name = "Grid"_kj;
    break;
  case StrategyType::Custom:
  default:
    type_name = "Custom"_kj;
    break;
  }

  // Lookup strategy factory using kj::HashMap find pattern
  KJ_IF_SOME(factory, lock->factories.find(type_name)) {
    // Create strategy instance
    kj::Rc<IStrategy> strategy = factory->create_strategy(config);
    if (strategy == nullptr) {
      logger_->error(kj::str("Failed to create strategy: ", config.name).cStr());
      return kj::str("");
    }

    // Initialize strategy
    if (!strategy->initialize(config, logger)) {
      logger_->error(kj::str("Failed to initialize strategy: ", config.name).cStr());
      return kj::str("");
    }

    // Store in thread-safe state using kj::HashMap upsert
    lock->strategies.upsert(kj::str(strategy_id), strategy.addRef(),
                            [](kj::Rc<IStrategy>&, kj::Rc<IStrategy>&&) {});

    // Also store in legacy map for backward compatibility using kj::HashMap upsert
    strategies_.upsert(kj::str(strategy_id), kj::mv(strategy),
                       [](kj::Rc<IStrategy>&, kj::Rc<IStrategy>&&) {});

    logger_->info(kj::str("Strategy loaded: ", strategy_id, " (", config.name, ")").cStr());
    return kj::mv(strategy_id);
  }

  logger_->error(kj::str("Strategy type not registered: ", type_name).cStr());
  return kj::str("");
}

bool StrategyManager::unload_strategy(kj::StringPtr strategy_id) {
  auto lock = state_.lockExclusive();

  // Use kj::HashMap find pattern
  KJ_IF_SOME(strategy, lock->strategies.find(strategy_id)) {
    // Stop strategy before unloading
    if (strategy != nullptr) {
      strategy->on_stop();
    }

    lock->strategies.erase(strategy_id);

    // Also remove from legacy map
    strategies_.erase(strategy_id);

    logger_->info(kj::str("Strategy unloaded: ", strategy_id).cStr());
    return true;
  }

  logger_->error(kj::str("Strategy not found for unload: ", strategy_id).cStr());
  return false;
}

bool StrategyManager::reload_parameters(kj::StringPtr strategy_id,
                                        const kj::TreeMap<kj::String, double>& parameters) {
  auto lock = state_.lockExclusive();

  // Use kj::HashMap find pattern
  KJ_IF_SOME(strategy, lock->strategies.find(strategy_id)) {
    if (strategy == nullptr) {
      logger_->error(kj::str("Strategy is null: ", strategy_id).cStr());
      return false;
    }

    if (!strategy->supports_hot_reload()) {
      logger_->warn(kj::str("Strategy does not support hot-reload: ", strategy_id).cStr());
      return false;
    }

    bool success = strategy->update_parameters(parameters);
    if (success) {
      logger_->info(kj::str("Parameters reloaded for strategy: ", strategy_id, " (",
                            parameters.size(), " params)")
                        .cStr());
    } else {
      logger_->error(kj::str("Failed to reload parameters for strategy: ", strategy_id).cStr());
    }

    return success;
  }

  logger_->error(kj::str("Strategy not found for parameter reload: ", strategy_id).cStr());
  return false;
}

void StrategyManager::set_signal_callback(
    kj::Function<void(const kj::Vector<veloz::exec::PlaceOrderRequest>&)> callback) {
  auto lock = state_.lockExclusive();
  lock->signal_callback = kj::mv(callback);
  logger_->info("Signal callback registered");
}

void StrategyManager::process_and_route_signals() {
  kj::Vector<veloz::exec::PlaceOrderRequest> all_signals;

  // Collect signals from all strategies
  {
    auto lock = state_.lockExclusive();
    // kj::HashMap iteration uses entry with .key and .value
    for (auto& entry : lock->strategies) {
      if (entry.value != nullptr) {
        // std::chrono for wall clock timestamps (KJ time is async I/O only)
        auto start = std::chrono::high_resolution_clock::now();
        auto signals = entry.value->get_signals();
        auto end = std::chrono::high_resolution_clock::now();

        // Log if there are signals
        if (signals.size() > 0) {
          auto duration_us =
              std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
          logger_->debug(kj::str("Strategy ", entry.key, " generated ", signals.size(),
                                 " signals in ", duration_us, " us")
                             .cStr());
        }

        for (size_t i = 0; i < signals.size(); ++i) {
          all_signals.add(kj::mv(signals[i]));
        }
      }
    }
  }

  // Route signals to callback if set
  if (all_signals.size() > 0) {
    auto lock = state_.lockExclusive();
    KJ_IF_SOME(callback, lock->signal_callback) {
      callback(all_signals);
    }
  }
}

kj::String StrategyManager::get_metrics_summary() const {
  auto lock = state_.lockShared();

  std::ostringstream oss;
  oss << "Strategy Metrics Summary:\n";
  oss << "========================\n";
  oss << "Total strategies: " << lock->strategies.size() << "\n";

  uint64_t total_events = 0;
  uint64_t total_signals = 0;
  uint64_t total_errors = 0;

  // kj::HashMap iteration uses entry with .key and .value
  for (const auto& entry : lock->strategies) {
    if (entry.value != nullptr) {
      KJ_IF_SOME(metrics, entry.value->get_metrics()) {
        total_events += metrics.events_processed.load();
        total_signals += metrics.signals_generated.load();
        total_errors += metrics.errors.load();

        oss << "\n" << entry.key.cStr() << ":\n";
        oss << "  Events: " << metrics.events_processed.load() << "\n";
        oss << "  Signals: " << metrics.signals_generated.load() << "\n";
        oss << "  Avg exec time: " << std::fixed << std::setprecision(2)
            << metrics.avg_execution_time_us() << " us\n";
        oss << "  Signals/sec: " << std::fixed << std::setprecision(2)
            << metrics.signals_per_second() << "\n";
        oss << "  Errors: " << metrics.errors.load() << "\n";
      }
      else {
        oss << "\n" << entry.key.cStr() << ": (no metrics)\n";
      }
    }
  }

  oss << "\nTotals:\n";
  oss << "  Events: " << total_events << "\n";
  oss << "  Signals: " << total_signals << "\n";
  oss << "  Errors: " << total_errors << "\n";

  return kj::str(oss.str().c_str());
}

kj::Maybe<const StrategyMetrics&>
StrategyManager::get_strategy_metrics(kj::StringPtr strategy_id) const {
  auto lock = state_.lockShared();

  // Use kj::HashMap find pattern
  KJ_IF_SOME(strategy, lock->strategies.find(strategy_id)) {
    if (strategy != nullptr) {
      return strategy->get_metrics();
    }
  }

  return kj::none;
}

bool StrategyManager::is_strategy_loaded(kj::StringPtr strategy_id) const {
  auto lock = state_.lockShared();
  // kj::HashMap find returns kj::Maybe
  return lock->strategies.find(strategy_id) != kj::none;
}

size_t StrategyManager::strategy_count() const {
  auto lock = state_.lockShared();
  return lock->strategies.size();
}

} // namespace veloz::strategy
