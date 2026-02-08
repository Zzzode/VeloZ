#pragma once

#include "veloz/market/market_event.h"
#include "veloz/market/subscription_manager.h"
#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <map>
#include <queue>
#include <condition_variable>
#include <mutex>

namespace veloz::market {

class BinanceWebSocket final {
public:
    using MarketEventCallback = std::function<void(const MarketEvent&)>;

    BinanceWebSocket(bool testnet = false);
    ~BinanceWebSocket();

    // Connection management
    bool connect();
    void disconnect();
    bool is_connected() const;

    // Subscribe to market events
    bool subscribe(const veloz::common::SymbolId& symbol, MarketEventType event_type);
    bool unsubscribe(const veloz::common::SymbolId& symbol, MarketEventType event_type);

    // Set callback for receiving market events
    void set_event_callback(MarketEventCallback callback);

    // Start/stop the WebSocket thread
    void start();
    void stop();

    // Get connection statistics
    [[nodiscard]] int get_reconnect_count() const;
    [[nodiscard]] std::int64_t get_last_message_time() const;
    [[nodiscard]] std::int64_t get_message_count() const;

private:
    // WebSocket thread function
    void run();

    // Internal helper methods
    std::string build_subscription_message(const veloz::common::SymbolId& symbol, MarketEventType event_type);
    std::string format_symbol(const veloz::common::SymbolId& symbol);
    std::string event_type_to_stream_name(MarketEventType event_type);
    MarketEventType parse_stream_name(const std::string& stream_name);

    // Message handling
    void handle_message(const std::string& message);
    MarketEvent parse_trade_message(const nlohmann::json& data, const veloz::common::SymbolId& symbol);
    MarketEvent parse_book_message(const nlohmann::json& data, const veloz::common::SymbolId& symbol, bool is_book_top);
    MarketEvent parse_kline_message(const nlohmann::json& data, const veloz::common::SymbolId& symbol);
    MarketEvent parse_ticker_message(const nlohmann::json& data, const veloz::common::SymbolId& symbol);

    // Connection state
    std::atomic<bool> connected_;
    std::atomic<bool> running_;

    // WebSocket thread
    std::unique_ptr<std::thread> thread_;

    // Event callback
    MarketEventCallback event_callback_;

    // Subscriptions
    std::map<veloz::common::SymbolId, std::vector<MarketEventType>> subscriptions_;
    mutable std::mutex subscriptions_mu_;

    // Connection parameters
    bool testnet_;
    std::string base_ws_url_;
    std::string stream_url_;

    // Statistics
    std::atomic<int> reconnect_count_;
    std::atomic<std::int64_t> last_message_time_;
    std::atomic<std::int64_t> message_count_;

    // Reconnect backoff
    std::atomic<int> current_backoff_;
    const int min_backoff_ms_;
    const int max_backoff_ms_;

    // Message queue for processing
    std::queue<std::string> message_queue_;
    std::mutex message_queue_mu_;
    std::condition_variable message_queue_cv_;
};

} // namespace veloz::market
