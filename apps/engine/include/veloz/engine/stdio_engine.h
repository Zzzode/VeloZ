#pragma once

#include "veloz/engine/engine_state.h"
#include "veloz/engine/event_emitter.h"
#include "veloz/exec/order_router.h"
#include "veloz/risk/risk_engine.h"
#include "veloz/market/order_book.h"
#include "veloz/market/subscription_manager.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <ostream>
#include <unordered_map>

namespace veloz::engine {

class StdioEngine final {
public:
    explicit StdioEngine(std::ostream& out);

    int run(std::atomic<bool>& stop_flag);

private:
    EngineState state_;
    EventEmitter emitter_;
    std::shared_ptr<veloz::exec::OrderRouter> order_router_;
    std::shared_ptr<veloz::risk::RiskEngine> risk_engine_;
    std::unordered_map<std::string, veloz::market::OrderBook> order_books_;
    std::shared_ptr<veloz::market::SubscriptionManager> subscription_manager_;
    mutable std::mutex market_mu_;

    void handle_market_event(const veloz::market::MarketEvent& event);
};

}
