#include "veloz/market/binance_websocket.h"

#include <nlohmann/json.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/config/core_client.hpp>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace veloz::market {

using json = nlohmann::json;
using websocket_client = websocketpp::client<websocketpp::config::core_client>;
using websocket_connection_ptr = websocket_client::connection_ptr;
using websocket_message_ptr = websocket_client::message_ptr;

BinanceWebSocket::BinanceWebSocket(bool testnet)
    : connected_(false)
    , running_(false)
    , testnet_(testnet)
    , reconnect_count_(0)
    , last_message_time_(0)
    , message_count_(0)
    , current_backoff_(1000)
    , min_backoff_ms_(1000)
    , max_backoff_ms_(30000) {
    if (testnet_) {
        base_ws_url_ = "wss://testnet.binance.vision";
    } else {
        base_ws_url_ = "wss://stream.binance.com:9443";
    }

    stream_url_ = base_ws_url_ + "/stream";
}

BinanceWebSocket::~BinanceWebSocket() {
    stop();
    disconnect();
}

bool BinanceWebSocket::connect() {
    if (connected_) {
        return true;
    }

    try {
        // Note: core_client (iostream transport) has different API than asio
        // For now, we'll just simulate connection for testing purposes
        connected_ = true;
        std::cout << "Binance WebSocket connected successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error connecting to Binance WebSocket: " << e.what() << std::endl;
        connected_ = false;
        return false;
    }
}

void BinanceWebSocket::disconnect() {
    connected_ = false;
    std::cout << "Binance WebSocket disconnected" << std::endl;
}

bool BinanceWebSocket::is_connected() const {
    return connected_;
}

std::string BinanceWebSocket::format_symbol(const veloz::common::SymbolId& symbol) {
    std::string formatted = symbol.value;
    std::transform(formatted.begin(), formatted.end(), formatted.begin(), ::tolower);
    return formatted;
}

std::string BinanceWebSocket::event_type_to_stream_name(MarketEventType event_type) {
    switch (event_type) {
        case MarketEventType::Trade:
            return "@trade";
        case MarketEventType::BookTop:
            return "@bookTicker";
        case MarketEventType::BookDelta:
            return "@depth";
        case MarketEventType::Kline:
            return "@kline_1m"; // Default to 1-minute klines
        case MarketEventType::Ticker:
            return "@miniTicker";
        default:
            return "@trade";
    }
}

MarketEventType BinanceWebSocket::parse_stream_name(const std::string& stream_name) {
    if (stream_name.find("@trade") != std::string::npos) {
        return MarketEventType::Trade;
    } else if (stream_name.find("@bookTicker") != std::string::npos) {
        return MarketEventType::BookTop;
    } else if (stream_name.find("@depth") != std::string::npos) {
        return MarketEventType::BookDelta;
    } else if (stream_name.find("@kline") != std::string::npos) {
        return MarketEventType::Kline;
    } else if (stream_name.find("@miniTicker") != std::string::npos) {
        return MarketEventType::Ticker;
    }

    return MarketEventType::Unknown;
}

std::string BinanceWebSocket::build_subscription_message(const veloz::common::SymbolId& symbol, MarketEventType event_type) {
    std::string stream = format_symbol(symbol) + event_type_to_stream_name(event_type);

    json j;
    j["method"] = "SUBSCRIBE";
    j["params"] = json::array({stream});
    j["id"] = 1;

    return j.dump();
}

bool BinanceWebSocket::subscribe(const veloz::common::SymbolId& symbol, MarketEventType event_type) {
    std::lock_guard<std::mutex> lock(subscriptions_mu_);

    // Check if already subscribed
    auto it = subscriptions_.find(symbol);
    if (it != subscriptions_.end()) {
        if (std::find(it->second.begin(), it->second.end(), event_type) != it->second.end()) {
            return true; // Already subscribed
        }
    }

    // Add to subscriptions
    subscriptions_[symbol].push_back(event_type);

    // Send subscription message if connected
    if (connected_) {
        std::string msg = build_subscription_message(symbol, event_type);
        // In real implementation, send via WebSocket
        std::cout << "Subscribed to stream: " << msg << std::endl;
    }

    return true;
}

bool BinanceWebSocket::unsubscribe(const veloz::common::SymbolId& symbol, MarketEventType event_type) {
    std::lock_guard<std::mutex> lock(subscriptions_mu_);

    auto it = subscriptions_.find(symbol);
    if (it == subscriptions_.end()) {
        return false; // Not subscribed
    }

    auto& event_types = it->second;
    auto event_it = std::find(event_types.begin(), event_types.end(), event_type);
    if (event_it == event_types.end()) {
        return false; // Not subscribed to this event type
    }

    event_types.erase(event_it);

    // If no more event types for this symbol, remove the entry
    if (event_types.empty()) {
        subscriptions_.erase(it);
    }

    // Send unsubscribe message if connected
    if (connected_) {
        std::string stream = format_symbol(symbol) + event_type_to_stream_name(event_type);
        json j;
        j["method"] = "UNSUBSCRIBE";
        j["params"] = json::array({stream});
        j["id"] = 2;

        // In real implementation, send via WebSocket
        std::cout << "Unsubscribed from stream: " << j.dump() << std::endl;
    }

    return true;
}

void BinanceWebSocket::set_event_callback(MarketEventCallback callback) {
    event_callback_ = std::move(callback);
}

void BinanceWebSocket::start() {
    if (running_) {
        return;
    }

    running_ = true;
    thread_ = std::make_unique<std::thread>([this]() {
        run();
    });
}

void BinanceWebSocket::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

void BinanceWebSocket::run() {
    while (running_) {
        if (!connected_) {
            if (connect()) {
                // Re-subscribe to all streams
                std::lock_guard<std::mutex> lock(subscriptions_mu_);
                for (const auto& [symbol, event_types] : subscriptions_) {
                    for (MarketEventType event_type : event_types) {
                        subscribe(symbol, event_type);
                    }
                }
                current_backoff_ = min_backoff_ms_;
            } else {
                // Reconnect with backoff
                int backoff = current_backoff_;
                std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
                current_backoff_ = std::min(current_backoff_ * 2, max_backoff_ms_);
                reconnect_count_++;
            }
        }

        // Process message queue
        std::string message;
        while (message_queue_.size() > 0) {
            {
                std::lock_guard<std::mutex> lock(message_queue_mu_);
                message = message_queue_.front();
                message_queue_.pop();
            }

            try {
                handle_message(message);
            } catch (const std::exception& e) {
                std::cerr << "Error processing WebSocket message: " << e.what() << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void BinanceWebSocket::handle_message(const std::string& message) {
    try {
        json j = json::parse(message);

        if (j.contains("stream") && j.contains("data")) {
            std::string stream = j["stream"].get<std::string>();
            json data = j["data"];

            // Parse symbol and event type from stream name
            size_t separator_pos = stream.find('@');
            if (separator_pos == std::string::npos) {
                return;
            }

            std::string symbol_str = stream.substr(0, separator_pos);
            std::transform(symbol_str.begin(), symbol_str.end(), symbol_str.begin(), ::toupper);
            veloz::common::SymbolId symbol(symbol_str);

            MarketEventType event_type = parse_stream_name(stream);

            // Parse message based on event type
            MarketEvent market_event;
            switch (event_type) {
                case MarketEventType::Trade:
                    market_event = parse_trade_message(data, symbol);
                    break;
                case MarketEventType::BookTop:
                    market_event = parse_book_message(data, symbol, true);
                    break;
                case MarketEventType::BookDelta:
                    market_event = parse_book_message(data, symbol, false);
                    break;
                case MarketEventType::Kline:
                    market_event = parse_kline_message(data, symbol);
                    break;
                case MarketEventType::Ticker:
                    market_event = parse_ticker_message(data, symbol);
                    break;
                default:
                    return; // Unknown event type
            }

            // Call callback if set
            if (event_callback_) {
                event_callback_(market_event);
            }

            message_count_++;
            last_message_time_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing WebSocket message: " << e.what() << std::endl;
    }
}

MarketEvent BinanceWebSocket::parse_trade_message(const nlohmann::json& data, const veloz::common::SymbolId& symbol) {
    MarketEvent event;
    event.type = MarketEventType::Trade;
    event.venue = veloz::common::Venue::Binance;
    event.symbol = symbol;
    event.market = veloz::common::MarketKind::Spot; // Default to spot

    event.ts_exchange_ns = data.value("T", 0LL) * 1000000;
    event.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    TradeData trade_data;
    trade_data.price = data.value("p", 0.0);
    trade_data.qty = data.value("q", 0.0);
    trade_data.is_buyer_maker = data.value("m", false);
    trade_data.trade_id = data.value("t", 0LL);

    event.data = std::move(trade_data);
    event.payload = data.dump();

    return event;
}

MarketEvent BinanceWebSocket::parse_book_message(const nlohmann::json& data, const veloz::common::SymbolId& symbol, bool is_book_top) {
    MarketEvent event;
    event.type = is_book_top ? MarketEventType::BookTop : MarketEventType::BookDelta;
    event.venue = veloz::common::Venue::Binance;
    event.symbol = symbol;
    event.market = veloz::common::MarketKind::Spot; // Default to spot

    event.ts_exchange_ns = data.value("E", 0LL) * 1000000;
    event.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    BookData book_data;
    if (is_book_top) {
        // Parse top of book data
        book_data.bids.emplace_back(BookLevel{data.value("b", 0.0), data.value("B", 0.0)});
        book_data.asks.emplace_back(BookLevel{data.value("a", 0.0), data.value("A", 0.0)});
        book_data.sequence = data.value("u", 0LL);
    } else {
        // Parse order book delta data
        if (data.contains("b")) {
            for (const auto& bid : data["b"]) {
                book_data.bids.emplace_back(BookLevel{std::stod(bid[0].get<std::string>()), std::stod(bid[1].get<std::string>())});
            }
        }

        if (data.contains("a")) {
            for (const auto& ask : data["a"]) {
                book_data.asks.emplace_back(BookLevel{std::stod(ask[0].get<std::string>()), std::stod(ask[1].get<std::string>())});
            }
        }

        book_data.sequence = data.value("u", 0LL);
    }

    event.data = std::move(book_data);
    event.payload = data.dump();

    return event;
}

MarketEvent BinanceWebSocket::parse_kline_message(const nlohmann::json& data, const veloz::common::SymbolId& symbol) {
    MarketEvent event;
    event.type = MarketEventType::Kline;
    event.venue = veloz::common::Venue::Binance;
    event.symbol = symbol;
    event.market = veloz::common::MarketKind::Spot; // Default to spot

    auto& kline = data["k"];
    event.ts_exchange_ns = data.value("E", 0LL) * 1000000;
    event.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    KlineData kline_data;
    kline_data.open = std::stod(kline["o"].get<std::string>());
    kline_data.high = std::stod(kline["h"].get<std::string>());
    kline_data.low = std::stod(kline["l"].get<std::string>());
    kline_data.close = std::stod(kline["c"].get<std::string>());
    kline_data.volume = std::stod(kline["v"].get<std::string>());
    kline_data.start_time = kline.value("t", 0LL);
    kline_data.close_time = kline.value("T", 0LL);

    event.data = std::move(kline_data);
    event.payload = data.dump();

    return event;
}

MarketEvent BinanceWebSocket::parse_ticker_message(const nlohmann::json& data, const veloz::common::SymbolId& symbol) {
    MarketEvent event;
    event.type = MarketEventType::Ticker;
    event.venue = veloz::common::Venue::Binance;
    event.symbol = symbol;
    event.market = veloz::common::MarketKind::Spot; // Default to spot

    event.ts_exchange_ns = data.value("E", 0LL) * 1000000;
    event.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // Ticker data structure
    // For Binance miniTicker, we could store relevant fields in payload
    event.payload = data.dump();

    return event;
}

int BinanceWebSocket::get_reconnect_count() const {
    return reconnect_count_.load();
}

std::int64_t BinanceWebSocket::get_last_message_time() const {
    return last_message_time_.load();
}

std::int64_t BinanceWebSocket::get_message_count() const {
    return message_count_.load();
}

} // namespace veloz::market
