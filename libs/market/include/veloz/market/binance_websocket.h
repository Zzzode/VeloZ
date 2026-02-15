#pragma once

#include "veloz/core/json.h"
#include "veloz/market/market_event.h"

#include <atomic> // std::atomic - KJ doesn't provide atomic primitives
#include <cstdint>
#include <cstring>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/compat/tls.h>
#include <kj/debug.h>
#include <kj/encoding.h>
#include <kj/function.h>
#include <kj/hash.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/timer.h>
#include <kj/vector.h>

namespace veloz::market {

/**
 * @brief WebSocket frame opcodes (RFC 6455)
 */
enum class WebSocketOpcode : std::uint8_t {
  Continuation = 0x0,
  Text = 0x1,
  Binary = 0x2,
  Close = 0x8,
  Ping = 0x9,
  Pong = 0xA
};

/**
 * @brief WebSocket frame structure for encoding/decoding
 */
struct WebSocketFrame {
  bool fin{true};
  WebSocketOpcode opcode{WebSocketOpcode::Text};
  bool masked{true}; // Client frames must be masked
  kj::Array<kj::byte> payload;
  std::uint32_t mask_key{0};
};

/**
 * @brief Binance WebSocket client implementation using KJ async I/O
 *
 * This class implements a WebSocket client connection to Binance using KJ's
 * async I/O primitives. It handles:
 *   - TCP socket connection via kj::AsyncIoProvider
 *   - SSL/TLS handshake using kj::TlsContext
 *   - WebSocket handshake (HTTP Upgrade request per RFC 6455)
 *   - WebSocket frame encoding/decoding
 *   - Automatic reconnection with exponential backoff
 *   - Ping/pong keepalive handling
 */
class BinanceWebSocket final {
public:
  using MarketEventCallback = kj::Function<void(const MarketEvent&)>;

  /**
   * @brief Construct a new BinanceWebSocket
   * @param ioContext KJ async I/O context for network operations
   * @param testnet Whether to connect to testnet (default: false)
   */
  BinanceWebSocket(kj::AsyncIoContext& ioContext, bool testnet = false);
  ~BinanceWebSocket();

  // Disable copy and move
  KJ_DISALLOW_COPY_AND_MOVE(BinanceWebSocket);

  // Connection management - async operations
  kj::Promise<void> connect();
  kj::Promise<void> disconnect();
  bool is_connected() const;

  // Subscribe to market events
  kj::Promise<bool> subscribe(const veloz::common::SymbolId& symbol, MarketEventType event_type);
  kj::Promise<bool> unsubscribe(const veloz::common::SymbolId& symbol, MarketEventType event_type);

  // Set callback for receiving market events
  void set_event_callback(MarketEventCallback callback);

  // Start/stop the WebSocket read loop
  kj::Promise<void> run();
  void stop();

  // Get connection statistics
  [[nodiscard]] int get_reconnect_count() const;
  [[nodiscard]] std::int64_t get_last_message_time() const;
  [[nodiscard]] std::int64_t get_message_count() const;

private:
  // Internal helper methods
  kj::String build_subscription_message(const veloz::common::SymbolId& symbol,
                                        MarketEventType event_type, bool subscribe);
  kj::String format_symbol(const veloz::common::SymbolId& symbol);
  kj::StringPtr event_type_to_stream_name(MarketEventType event_type);
  MarketEventType parse_stream_name(kj::StringPtr stream_name);

  // Message handling
  void handle_message(kj::StringPtr message);
  MarketEvent parse_trade_message(const veloz::core::JsonValue& data,
                                  const veloz::common::SymbolId& symbol);
  MarketEvent parse_book_message(const veloz::core::JsonValue& data,
                                 const veloz::common::SymbolId& symbol, bool is_book_top);
  MarketEvent parse_kline_message(const veloz::core::JsonValue& data,
                                  const veloz::common::SymbolId& symbol);
  MarketEvent parse_ticker_message(const veloz::core::JsonValue& data,
                                   const veloz::common::SymbolId& symbol);

  // Resubscribe to all streams after reconnection
  kj::Promise<void> resubscribe_all();

  // Schedule reconnection with exponential backoff
  kj::Promise<void> schedule_reconnect();
  void reset_reconnect_state();

  // KJ async I/O operations
  kj::String build_websocket_handshake();
  kj::String generate_websocket_key();
  bool validate_handshake_response(kj::StringPtr response, kj::StringPtr expectedAccept);
  kj::String compute_accept_key(kj::StringPtr key);

  // WebSocket frame operations
  kj::Array<kj::byte> encode_websocket_frame(kj::ArrayPtr<const kj::byte> payload,
                                             WebSocketOpcode opcode = WebSocketOpcode::Text,
                                             bool mask = true);
  kj::Promise<WebSocketFrame> read_websocket_frame();
  kj::Promise<WebSocketFrame> read_extended_length_16(WebSocketFrame frame);
  kj::Promise<WebSocketFrame> read_extended_length_64(WebSocketFrame frame);
  kj::Promise<WebSocketFrame> read_frame_payload(WebSocketFrame frame, uint64_t payload_len);
  kj::Promise<WebSocketFrame> read_mask_key(WebSocketFrame frame, uint64_t payload_len);
  kj::Promise<WebSocketFrame> read_masked_payload(WebSocketFrame frame, uint64_t payload_len);
  kj::Promise<WebSocketFrame> read_unmasked_payload(WebSocketFrame frame, uint64_t payload_len);
  kj::Promise<void> send_websocket_frame(kj::ArrayPtr<const kj::byte> payload,
                                         WebSocketOpcode opcode = WebSocketOpcode::Text);

  // Send text message
  kj::Promise<void> send_text(kj::StringPtr message);

  // Ping/pong handling
  kj::Promise<void> send_ping();
  kj::Promise<void> send_pong(kj::ArrayPtr<const kj::byte> payload);

  // Main read loop
  kj::Promise<void> read_loop();

  // Connection state
  std::atomic<bool> connected_;
  std::atomic<bool> running_;

  // KJ async I/O context and stream
  kj::AsyncIoContext& ioContext_;
  kj::Own<kj::TlsContext> tlsContext_;
  kj::Maybe<kj::Own<kj::AsyncIoStream>> stream_;

  // Event callback - protected by KJ mutex
  struct CallbackState {
    kj::Maybe<MarketEventCallback> callback;
  };
  kj::MutexGuarded<CallbackState> callback_state_;

  // Subscriptions - using kj::HashMap with symbol string key and kj::Vector for event types
  struct SubscriptionState {
    kj::HashMap<kj::String, kj::Vector<MarketEventType>> subscriptions;
    std::atomic<int> next_subscription_id; // std::atomic - KJ doesn't provide atomic primitives
  };
  kj::MutexGuarded<SubscriptionState> subscription_state_;

  // Connection parameters
  bool testnet_;
  kj::String host_;
  uint16_t port_;
  kj::String path_;

  // WebSocket handshake key (stored for validation)
  kj::String websocket_key_;

  // Reconnection parameters
  std::atomic<std::int64_t> reconnect_delay_ms_;
  std::atomic<int> reconnect_attempts_;

  // Statistics
  std::atomic<int> reconnect_count_;
  std::atomic<std::int64_t> last_message_time_;
  std::atomic<std::int64_t> message_count_;

  // Read buffer for incoming data
  kj::Vector<kj::byte> read_buffer_;

  // Message fragment accumulator for fragmented messages
  kj::Vector<kj::byte> fragment_buffer_;
  WebSocketOpcode fragment_opcode_;
};

} // namespace veloz::market
