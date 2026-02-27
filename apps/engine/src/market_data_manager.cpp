#include "veloz/engine/market_data_manager.h"

#include <chrono> // std::chrono - KJ timer uses different time representation
#include <kj/debug.h>

namespace veloz::engine {

MarketDataManager::MarketDataManager(kj::AsyncIoContext& ioContext, EventEmitter& emitter,
                                     Config config, kj::Maybe<veloz::core::EventLoop&> event_loop)
    : config_(config), ioContext_(ioContext), emitter_(emitter), event_loop_(event_loop) {
  KJ_LOG(INFO, "MarketDataManager initialized", config_.use_testnet ? "testnet" : "mainnet");
}

MarketDataManager::~MarketDataManager() {
  stop();
}

kj::Promise<void> MarketDataManager::start() {
  if (running_.load()) {
    return kj::READY_NOW;
  }

  running_ = true;
  KJ_LOG(INFO, "MarketDataManager starting");

  // Initialize Binance WebSocket
  auto binance = kj::heap<veloz::market::BinanceWebSocket>(ioContext_, config_.use_testnet);

  // Set up event callback to route events to emitter
  binance->set_event_callback(
      [this](const veloz::market::MarketEvent& event) { on_market_event(event); });

  binance_ws_ = kj::mv(binance);

  // Start the WebSocket connection and read loop
  KJ_IF_SOME(ws, binance_ws_) {
    return ws->run();
  }

  return kj::READY_NOW;
}

void MarketDataManager::stop() {
  if (!running_.load()) {
    return;
  }

  KJ_LOG(INFO, "MarketDataManager stopping");
  running_ = false;

  // Stop Binance WebSocket
  KJ_IF_SOME(ws, binance_ws_) {
    ws->stop();
  }
}

bool MarketDataManager::is_running() const {
  return running_.load();
}

kj::Promise<bool> MarketDataManager::subscribe(kj::StringPtr venue,
                                               const veloz::common::SymbolId& symbol,
                                               veloz::market::MarketEventType event_type) {
  // Helper to convert char to lowercase
  auto toLower = [](char c) -> char { return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c; };

  // Check venue (case-insensitive)
  bool is_binance = true;
  kj::StringPtr binance_str = "binance"_kj;
  if (venue.size() != binance_str.size()) {
    is_binance = false;
  } else {
    for (size_t i = 0; i < venue.size(); ++i) {
      if (toLower(venue[i]) != binance_str[i]) {
        is_binance = false;
        break;
      }
    }
  }

  if (is_binance) {
    KJ_IF_SOME(ws, binance_ws_) {
      auto ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();

      return ws->subscribe(symbol, event_type)
          .then([this, symbol, event_type, ts_ns](bool success) {
            if (success) {
              total_subscriptions_++;
              // Emit subscription status
              kj::StringPtr event_type_str = "unknown"_kj;
              switch (event_type) {
              case veloz::market::MarketEventType::Trade:
                event_type_str = "trade"_kj;
                break;
              case veloz::market::MarketEventType::BookTop:
                event_type_str = "book_top"_kj;
                break;
              case veloz::market::MarketEventType::BookDelta:
                event_type_str = "book_delta"_kj;
                break;
              case veloz::market::MarketEventType::Kline:
                event_type_str = "kline"_kj;
                break;
              case veloz::market::MarketEventType::Ticker:
                event_type_str = "ticker"_kj;
                break;
              default:
                break;
              }
              emitter_.emit_subscription_status(symbol.value, event_type_str, "subscribed"_kj,
                                                ts_ns);
            }
            return success;
          });
    }
  }

  KJ_LOG(WARNING, "Unknown venue for subscription", venue);
  return kj::Promise<bool>(false);
}

kj::Promise<bool> MarketDataManager::unsubscribe(kj::StringPtr venue,
                                                 const veloz::common::SymbolId& symbol,
                                                 veloz::market::MarketEventType event_type) {
  // Helper to convert char to lowercase
  auto toLower = [](char c) -> char { return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c; };

  // Check venue (case-insensitive)
  bool is_binance = true;
  kj::StringPtr binance_str = "binance"_kj;
  if (venue.size() != binance_str.size()) {
    is_binance = false;
  } else {
    for (size_t i = 0; i < venue.size(); ++i) {
      if (toLower(venue[i]) != binance_str[i]) {
        is_binance = false;
        break;
      }
    }
  }

  if (is_binance) {
    KJ_IF_SOME(ws, binance_ws_) {
      auto ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();

      return ws->unsubscribe(symbol, event_type)
          .then([this, symbol, event_type, ts_ns](bool success) {
            if (success) {
              total_subscriptions_--;
              // Emit subscription status
              kj::StringPtr event_type_str = "unknown"_kj;
              switch (event_type) {
              case veloz::market::MarketEventType::Trade:
                event_type_str = "trade"_kj;
                break;
              case veloz::market::MarketEventType::BookTop:
                event_type_str = "book_top"_kj;
                break;
              case veloz::market::MarketEventType::BookDelta:
                event_type_str = "book_delta"_kj;
                break;
              case veloz::market::MarketEventType::Kline:
                event_type_str = "kline"_kj;
                break;
              case veloz::market::MarketEventType::Ticker:
                event_type_str = "ticker"_kj;
                break;
              default:
                break;
              }
              emitter_.emit_subscription_status(symbol.value, event_type_str, "unsubscribed"_kj,
                                                ts_ns);
            }
            return success;
          });
    }
  }

  KJ_LOG(WARNING, "Unknown venue for unsubscription", venue);
  return kj::Promise<bool>(false);
}

bool MarketDataManager::is_venue_connected(kj::StringPtr venue) const {
  // Helper to convert char to lowercase
  auto toLower = [](char c) -> char { return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c; };

  // Check venue (case-insensitive)
  bool is_binance = true;
  kj::StringPtr binance_str = "binance"_kj;
  if (venue.size() != binance_str.size()) {
    is_binance = false;
  } else {
    for (size_t i = 0; i < venue.size(); ++i) {
      if (toLower(venue[i]) != binance_str[i]) {
        is_binance = false;
        break;
      }
    }
  }

  if (is_binance) {
    KJ_IF_SOME(ws, binance_ws_) {
      return ws->is_connected();
    }
  }

  return false;
}

std::int64_t MarketDataManager::total_events_received() const {
  return total_events_.load();
}

std::int64_t MarketDataManager::total_subscriptions() const {
  return total_subscriptions_.load();
}

void MarketDataManager::on_market_event(const veloz::market::MarketEvent& event) {
  total_events_++;
  KJ_IF_SOME(loop, event_loop_) {
    if (loop.is_running()) {
      auto tags = build_market_event_tags(event);
      auto priority = market_event_priority(event);
      auto copy = clone_market_event(event);
      loop.post_with_tags([this, event = kj::mv(copy)]() mutable { emitter_.emit_market_event(event); },
                          priority, kj::mv(tags));
      return;
    }
  }
  emitter_.emit_market_event(event);
}

kj::Maybe<veloz::market::BinanceWebSocket&> MarketDataManager::get_binance_websocket() {
  KJ_IF_SOME(ws, binance_ws_) {
    return *ws;
  }
  return kj::none;
}

kj::Vector<veloz::core::EventTag>
MarketDataManager::build_market_event_tags(const veloz::market::MarketEvent& event) const {
  kj::Vector<veloz::core::EventTag> tags;
  tags.add(kj::str("market"));
  tags.add(kj::str("venue:", veloz::common::to_string(event.venue)));
  tags.add(kj::str("symbol:", event.symbol.value));

  kj::StringPtr type_tag = "unknown"_kj;
  switch (event.type) {
  case veloz::market::MarketEventType::Trade:
    type_tag = "trade"_kj;
    break;
  case veloz::market::MarketEventType::BookTop:
    type_tag = "book_top"_kj;
    break;
  case veloz::market::MarketEventType::BookDelta:
    type_tag = "book_delta"_kj;
    break;
  case veloz::market::MarketEventType::Kline:
    type_tag = "kline"_kj;
    break;
  case veloz::market::MarketEventType::Ticker:
    type_tag = "ticker"_kj;
    break;
  case veloz::market::MarketEventType::FundingRate:
    type_tag = "funding_rate"_kj;
    break;
  case veloz::market::MarketEventType::MarkPrice:
    type_tag = "mark_price"_kj;
    break;
  default:
    break;
  }
  tags.add(kj::str("type:", type_tag));
  return tags;
}

veloz::core::EventPriority
MarketDataManager::market_event_priority(const veloz::market::MarketEvent& event) const {
  switch (event.type) {
  case veloz::market::MarketEventType::Trade:
    return veloz::core::EventPriority::High;
  case veloz::market::MarketEventType::BookDelta:
    return veloz::core::EventPriority::High;
  case veloz::market::MarketEventType::BookTop:
    return veloz::core::EventPriority::Normal;
  case veloz::market::MarketEventType::Ticker:
    return veloz::core::EventPriority::Normal;
  case veloz::market::MarketEventType::MarkPrice:
    return veloz::core::EventPriority::Normal;
  case veloz::market::MarketEventType::Kline:
    return veloz::core::EventPriority::Low;
  case veloz::market::MarketEventType::FundingRate:
    return veloz::core::EventPriority::Low;
  default:
    return veloz::core::EventPriority::Low;
  }
}

veloz::market::MarketEvent
MarketDataManager::clone_market_event(const veloz::market::MarketEvent& event) const {
  veloz::market::MarketEvent copy;
  copy.type = event.type;
  copy.venue = event.venue;
  copy.market = event.market;
  copy.symbol = event.symbol;
  copy.ts_exchange_ns = event.ts_exchange_ns;
  copy.ts_recv_ns = event.ts_recv_ns;
  copy.ts_pub_ns = event.ts_pub_ns;
  copy.payload = kj::heapString(event.payload);

  if (event.data.is<veloz::market::TradeData>()) {
    copy.data = event.data.get<veloz::market::TradeData>();
  } else if (event.data.is<veloz::market::BookData>()) {
    const auto& source = event.data.get<veloz::market::BookData>();
    veloz::market::BookData book;
    book.sequence = source.sequence;
    book.first_update_id = source.first_update_id;
    book.is_snapshot = source.is_snapshot;
    for (auto level : source.bids) {
      book.bids.add(level);
    }
    for (auto level : source.asks) {
      book.asks.add(level);
    }
    copy.data = kj::mv(book);
  } else if (event.data.is<veloz::market::KlineData>()) {
    copy.data = event.data.get<veloz::market::KlineData>();
  } else {
    copy.data = veloz::market::EmptyData{};
  }

  return copy;
}

} // namespace veloz::engine
