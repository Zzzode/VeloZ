#include "veloz/core/logger.h"
#include "veloz/strategy/strategy.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>

namespace veloz::strategy {

StrategyManager::StrategyManager() {
  logger_ = std::make_shared<core::Logger>();
}

StrategyManager::~StrategyManager() {
  // Stop all strategies
  for (auto& [id, strategy] : strategies_) {
    if (strategy) {
      strategy->on_stop();
    }
  }
  strategies_.clear();
}

void StrategyManager::register_strategy_factory(const std::shared_ptr<IStrategyFactory>& factory) {
  if (!factory) {
    throw std::invalid_argument("Factory cannot be null");
  }

  std::string type_name(factory->get_strategy_type().cStr());
  if (factories_.contains(type_name)) {
    logger_->warn(std::string("Strategy type ") + type_name + " already registered");
    return;
  }

  factories_[type_name] = factory;
  logger_->info(std::string("Strategy type ") + type_name + " registered successfully");
}

std::shared_ptr<IStrategy> StrategyManager::create_strategy(const StrategyConfig& config) {
  // Generate unique strategy ID
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(100000, 999999);

  std::string strategy_id = "strat-" + std::to_string(dis(gen));

  // Convert StrategyType to string for factory lookup
  std::string type_name;
  switch (config.type) {
  case StrategyType::TrendFollowing:
    type_name = "TrendFollowing";
    break;
  case StrategyType::MeanReversion:
    type_name = "MeanReversion";
    break;
  case StrategyType::Momentum:
    type_name = "Momentum";
    break;
  case StrategyType::Arbitrage:
    type_name = "Arbitrage";
    break;
  case StrategyType::MarketMaking:
    type_name = "MarketMaking";
    break;
  case StrategyType::Grid:
    type_name = "Grid";
    break;
  case StrategyType::Custom:
  default:
    type_name = "Custom";
    break;
  }

  // Lookup strategy factory
  auto factory_it = factories_.find(type_name);
  if (factory_it == factories_.end()) {
    logger_->error("Strategy type not registered: " + type_name);
    return nullptr;
  }

  // Create strategy instance
  auto strategy = factory_it->second->create_strategy(config);
  if (strategy) {
    strategies_[strategy_id] = strategy;
    logger_->info("Strategy created: " + strategy_id + " (" + std::string(config.name.cStr()) +
                  ")");
  }

  return strategy;
}

bool StrategyManager::start_strategy(const std::string& strategy_id) {
  auto it = strategies_.find(strategy_id);
  if (it == strategies_.end()) {
    logger_->error("Strategy not found: " + strategy_id);
    return false;
  }

  if (it->second) {
    it->second->on_start();
    logger_->info("Strategy started: " + strategy_id);
    return true;
  }

  return false;
}

bool StrategyManager::stop_strategy(const std::string& strategy_id) {
  auto it = strategies_.find(strategy_id);
  if (it == strategies_.end()) {
    logger_->error("Strategy not found: " + strategy_id);
    return false;
  }

  if (it->second) {
    it->second->on_stop();
    logger_->info("Strategy stopped: " + strategy_id);
    return true;
  }

  return false;
}

bool StrategyManager::remove_strategy(const std::string& strategy_id) {
  auto it = strategies_.find(strategy_id);
  if (it == strategies_.end()) {
    logger_->error("Strategy not found: " + strategy_id);
    return false;
  }

  if (it->second) {
    it->second->on_stop();
  }

  strategies_.erase(it);
  logger_->info("Strategy removed: " + strategy_id);
  return true;
}

std::shared_ptr<IStrategy> StrategyManager::get_strategy(const std::string& strategy_id) const {
  auto it = strategies_.find(strategy_id);
  if (it == strategies_.end()) {
    logger_->error("Strategy not found: " + strategy_id);
    return nullptr;
  }

  return it->second;
}

kj::Vector<StrategyState> StrategyManager::get_all_strategy_states() const {
  kj::Vector<StrategyState> states;

  for (const auto& [id, strategy] : strategies_) {
    if (strategy) {
      states.add(strategy->get_state());
    }
  }

  return states;
}

kj::Vector<kj::String> StrategyManager::get_all_strategy_ids() const {
  kj::Vector<kj::String> ids;

  for (const auto& [id, _] : strategies_) {
    ids.add(kj::heapString(id));
  }

  return ids;
}

void StrategyManager::on_market_event(const veloz::market::MarketEvent& event) {
  for (auto& [id, strategy] : strategies_) {
    if (strategy) {
      strategy->on_event(event);
    }
  }
}

void StrategyManager::on_position_update(const veloz::oms::Position& position) {
  for (auto& [id, strategy] : strategies_) {
    if (strategy) {
      strategy->on_position_update(position);
    }
  }
}

void StrategyManager::on_timer(int64_t timestamp) {
  for (auto& [id, strategy] : strategies_) {
    if (strategy) {
      strategy->on_timer(timestamp);
    }
  }
}

kj::Vector<veloz::exec::PlaceOrderRequest> StrategyManager::get_all_signals() {
  kj::Vector<veloz::exec::PlaceOrderRequest> all_signals;

  for (auto& [id, strategy] : strategies_) {
    if (strategy) {
      auto signals = strategy->get_signals();
      for (size_t i = 0; i < signals.size(); ++i) {
        all_signals.add(kj::mv(signals[i]));
      }
    }
  }

  return all_signals;
}

} // namespace veloz::strategy
