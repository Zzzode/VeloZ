#include "veloz/market/binance_websocket.h"

// KJ library includes for async I/O and networking
// Note: std::stod used for string-to-double conversion (no KJ equivalent)
#include <chrono>  // std::chrono - KJ timer uses different time representation
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/encoding.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/timer.h>
#include <random>  // std::random_device, std::mt19937 - KJ doesn't provide RNG

namespace veloz::market {

using namespace veloz::core;

namespace {

// WebSocket magic GUID for Sec-WebSocket-Accept computation (RFC 6455)
constexpr kj::StringPtr WS_MAGIC_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"_kj;

// Generate a random 32-bit mask key for WebSocket frames
std::uint32_t generate_mask_key() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<std::uint32_t> dis(0, 0xFFFFFFFF);
  return dis(gen);
}

// Simple SHA-1 implementation for WebSocket handshake
// Note: In production, use a proper crypto library
class SHA1 {
public:
  SHA1() {
    reset();
  }

  void reset() {
    h0_ = 0x67452301;
    h1_ = 0xEFCDAB89;
    h2_ = 0x98BADCFE;
    h3_ = 0x10325476;
    h4_ = 0xC3D2E1F0;
    total_bits_ = 0;
    buffer_size_ = 0;
  }

  void update(kj::ArrayPtr<const kj::byte> data) {
    const auto* bytes = data.begin();
    size_t len = data.size();
    total_bits_ += len * 8;

    while (len > 0) {
      size_t to_copy = kj::min(len, 64 - buffer_size_);
      std::memcpy(buffer_ + buffer_size_, bytes, to_copy);
      buffer_size_ += to_copy;
      bytes += to_copy;
      len -= to_copy;

      if (buffer_size_ == 64) {
        process_block();
        buffer_size_ = 0;
      }
    }
  }

  kj::Array<kj::byte> finalize() {
    // Padding
    buffer_[buffer_size_++] = 0x80;
    if (buffer_size_ > 56) {
      while (buffer_size_ < 64)
        buffer_[buffer_size_++] = 0;
      process_block();
      buffer_size_ = 0;
    }
    while (buffer_size_ < 56)
      buffer_[buffer_size_++] = 0;

    // Append length in bits (big-endian)
    for (int i = 7; i >= 0; --i) {
      buffer_[buffer_size_++] = static_cast<uint8_t>(total_bits_ >> (i * 8));
    }
    process_block();

    // Output hash
    auto result = kj::heapArray<kj::byte>(20);
    for (int i = 0; i < 4; ++i) {
      result[i] = static_cast<kj::byte>(h0_ >> (24 - i * 8));
      result[4 + i] = static_cast<kj::byte>(h1_ >> (24 - i * 8));
      result[8 + i] = static_cast<kj::byte>(h2_ >> (24 - i * 8));
      result[12 + i] = static_cast<kj::byte>(h3_ >> (24 - i * 8));
      result[16 + i] = static_cast<kj::byte>(h4_ >> (24 - i * 8));
    }
    return result;
  }

private:
  void process_block() {
    uint32_t w[80];

    // Break chunk into sixteen 32-bit big-endian words
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<uint32_t>(buffer_[i * 4]) << 24) |
             (static_cast<uint32_t>(buffer_[i * 4 + 1]) << 16) |
             (static_cast<uint32_t>(buffer_[i * 4 + 2]) << 8) |
             static_cast<uint32_t>(buffer_[i * 4 + 3]);
    }

    // Extend
    for (int i = 16; i < 80; ++i) {
      uint32_t val = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
      w[i] = (val << 1) | (val >> 31);
    }

    uint32_t a = h0_, b = h1_, c = h2_, d = h3_, e = h4_;

    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }

      uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
      e = d;
      d = c;
      c = (b << 30) | (b >> 2);
      b = a;
      a = temp;
    }

    h0_ += a;
    h1_ += b;
    h2_ += c;
    h3_ += d;
    h4_ += e;
  }

  uint32_t h0_, h1_, h2_, h3_, h4_;
  uint64_t total_bits_;
  uint8_t buffer_[64];
  size_t buffer_size_;
};

} // anonymous namespace

BinanceWebSocket::BinanceWebSocket(kj::AsyncIoContext& ioContext, bool testnet)
    : connected_(false), running_(false), ioContext_(ioContext), testnet_(testnet),
      reconnect_count_(0), last_message_time_(0), message_count_(0), reconnect_delay_ms_(1000),
      reconnect_attempts_(0), fragment_opcode_(WebSocketOpcode::Text) {

  // Initialize TLS context with default options (uses system trust store)
  kj::TlsContext::Options tlsOptions;
  tlsContext_ = kj::heap<kj::TlsContext>(kj::mv(tlsOptions));

  // Determine URL based on testnet flag
  if (testnet_) {
    host_ = kj::heapString("testnet.binance.vision");
    port_ = 443;
  } else {
    host_ = kj::heapString("stream.binance.com");
    port_ = 9443;
  }
  path_ = kj::heapString("/stream");

  KJ_LOG(INFO, "BinanceWebSocket initialized", testnet_ ? "testnet" : "mainnet", host_, port_,
         path_);
}

BinanceWebSocket::~BinanceWebSocket() {
  running_ = false;
  connected_ = false;
}

kj::String BinanceWebSocket::generate_websocket_key() {
  // Generate 16 random bytes and base64 encode them
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dis(0, 255);

  auto bytes = kj::heapArray<kj::byte>(16);
  for (size_t i = 0; i < 16; ++i) {
    bytes[i] = static_cast<kj::byte>(dis(gen));
  }

  return kj::encodeBase64(bytes);
}

kj::String BinanceWebSocket::compute_accept_key(kj::StringPtr key) {
  // Compute SHA-1 hash of key + magic GUID
  kj::String combined = kj::str(key, WS_MAGIC_GUID);

  SHA1 sha1;
  sha1.update(combined.asBytes());
  auto hash = sha1.finalize();

  return kj::encodeBase64(hash);
}

kj::String BinanceWebSocket::build_websocket_handshake() {
  // Generate a new WebSocket key for this connection
  websocket_key_ = generate_websocket_key();

  // Build HTTP Upgrade request per RFC 6455
  return kj::str("GET ", path_,
                 " HTTP/1.1\r\n"
                 "Host: ",
                 host_, ":", port_,
                 "\r\n"
                 "Upgrade: websocket\r\n"
                 "Connection: Upgrade\r\n"
                 "Sec-WebSocket-Key: ",
                 websocket_key_,
                 "\r\n"
                 "Sec-WebSocket-Version: 13\r\n"
                 "\r\n");
}

bool BinanceWebSocket::validate_handshake_response(kj::StringPtr response,
                                                   kj::StringPtr expectedAccept) {
  // Check for HTTP 101 Switching Protocols
  if (response.size() < 12)
    return false;

  // Helper lambda to convert char to lowercase
  auto toLower = [](char c) -> char {
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
  };

  // Helper lambda to find substring (case-insensitive)
  auto findCaseInsensitive = [&](kj::StringPtr haystack, kj::StringPtr needle) -> kj::Maybe<size_t> {
    if (needle.size() > haystack.size()) return kj::none;
    for (size_t i = 0; i <= haystack.size() - needle.size(); ++i) {
      bool match = true;
      for (size_t j = 0; j < needle.size(); ++j) {
        if (toLower(haystack[i + j]) != toLower(needle[j])) {
          match = false;
          break;
        }
      }
      if (match) return i;
    }
    return kj::none;
  };

  // Helper lambda to find substring (case-sensitive)
  auto findSubstring = [](kj::StringPtr haystack, kj::StringPtr needle) -> kj::Maybe<size_t> {
    if (needle.size() > haystack.size()) return kj::none;
    for (size_t i = 0; i <= haystack.size() - needle.size(); ++i) {
      bool match = true;
      for (size_t j = 0; j < needle.size(); ++j) {
        if (haystack[i + j] != needle[j]) {
          match = false;
          break;
        }
      }
      if (match) return i;
    }
    return kj::none;
  };

  // Check status line
  bool has101 = false;
  KJ_IF_SOME(pos, findSubstring(response, "HTTP/1.1 101"_kj)) {
    has101 = true;
    (void)pos;
  }
  if (!has101) {
    KJ_IF_SOME(pos, findSubstring(response, "HTTP/1.0 101"_kj)) {
      has101 = true;
      (void)pos;
    }
  }
  if (!has101) {
    KJ_LOG(ERROR, "WebSocket handshake failed: not 101 response");
    return false;
  }

  // Check for Upgrade: websocket header (case-insensitive)
  KJ_IF_SOME(pos, findCaseInsensitive(response, "upgrade: websocket"_kj)) {
    (void)pos;
  } else {
    KJ_LOG(ERROR, "WebSocket handshake failed: missing Upgrade header");
    return false;
  }

  // Check for Connection: Upgrade header (case-insensitive)
  KJ_IF_SOME(pos, findCaseInsensitive(response, "connection: upgrade"_kj)) {
    (void)pos;
  } else {
    KJ_LOG(ERROR, "WebSocket handshake failed: missing Connection header");
    return false;
  }

  // Check Sec-WebSocket-Accept header (case-insensitive search for header name)
  kj::StringPtr accept_header = "sec-websocket-accept: "_kj;
  KJ_IF_SOME(accept_pos, findCaseInsensitive(response, accept_header)) {
    // Extract accept value - need to find the actual position in original string
    // since we need case-sensitive value extraction
    size_t value_start = accept_pos + accept_header.size();

    // Find end of line
    size_t value_end = response.size();
    for (size_t i = value_start; i < response.size() - 1; ++i) {
      if (response[i] == '\r' && response[i + 1] == '\n') {
        value_end = i;
        break;
      }
    }

    // Extract and trim accept value
    kj::String accept_value = kj::heapString(response.slice(value_start, value_end));

    // Trim trailing whitespace
    size_t end = accept_value.size();
    while (end > 0 && (accept_value[end - 1] == ' ' || accept_value[end - 1] == '\t')) {
      --end;
    }

    // Trim leading whitespace
    size_t start = 0;
    while (start < end && (accept_value[start] == ' ' || accept_value[start] == '\t')) {
      ++start;
    }

    auto sliced = accept_value.slice(start, end);
    kj::String trimmed = kj::str(sliced);

    // Validate accept key
    if (trimmed != expectedAccept) {
      KJ_LOG(ERROR, "WebSocket handshake failed: invalid Sec-WebSocket-Accept", trimmed.cStr(),
             expectedAccept.cStr());
      return false;
    }

    return true;
  } else {
    KJ_LOG(ERROR, "WebSocket handshake failed: missing Sec-WebSocket-Accept header");
    return false;
  }
}

kj::Array<kj::byte> BinanceWebSocket::encode_websocket_frame(kj::ArrayPtr<const kj::byte> payload,
                                                             WebSocketOpcode opcode, bool mask) {

  size_t payload_len = payload.size();
  size_t header_size = 2;

  if (payload_len > 125 && payload_len <= 65535) {
    header_size += 2;
  } else if (payload_len > 65535) {
    header_size += 8;
  }

  if (mask) {
    header_size += 4;
  }

  auto frame = kj::heapArray<kj::byte>(header_size + payload_len);
  size_t offset = 0;

  frame[offset++] = static_cast<kj::byte>(0x80 | static_cast<uint8_t>(opcode));

  uint8_t mask_bit = mask ? 0x80 : 0x00;
  if (payload_len <= 125) {
    frame[offset++] = static_cast<kj::byte>(mask_bit | payload_len);
  } else if (payload_len <= 65535) {
    frame[offset++] = static_cast<kj::byte>(mask_bit | 126);
    frame[offset++] = static_cast<kj::byte>((payload_len >> 8) & 0xFF);
    frame[offset++] = static_cast<kj::byte>(payload_len & 0xFF);
  } else {
    frame[offset++] = static_cast<kj::byte>(mask_bit | 127);
    for (int i = 7; i >= 0; --i) {
      frame[offset++] = static_cast<kj::byte>((payload_len >> (i * 8)) & 0xFF);
    }
  }

  // Add mask key and mask payload if required
  if (mask) {
    uint32_t mask_key = generate_mask_key();
    frame[offset++] = static_cast<kj::byte>((mask_key >> 24) & 0xFF);
    frame[offset++] = static_cast<kj::byte>((mask_key >> 16) & 0xFF);
    frame[offset++] = static_cast<kj::byte>((mask_key >> 8) & 0xFF);
    frame[offset++] = static_cast<kj::byte>(mask_key & 0xFF);

    // Copy and mask payload
    for (size_t i = 0; i < payload_len; ++i) {
      uint8_t mask_byte = static_cast<uint8_t>((mask_key >> (8 * (3 - (i % 4)))) & 0xFF);
      frame[offset + i] = static_cast<kj::byte>(static_cast<uint8_t>(payload[i]) ^ mask_byte);
    }
  } else {
    // Copy payload without masking
    std::memcpy(frame.begin() + offset, payload.begin(), payload_len);
  }

  return frame;
}

kj::Promise<WebSocketFrame> BinanceWebSocket::read_websocket_frame() {
  KJ_IF_SOME(streamOwn, stream_) {
    auto& stream = *streamOwn;

    // Read at least 2 bytes for the frame header
    auto headerBuf = kj::heapArray<kj::byte>(2);
    return stream.read(headerBuf.asPtr(), 2)
        .then([this, headerBuf = kj::mv(headerBuf)](
                  size_t bytesRead) mutable -> kj::Promise<WebSocketFrame> {
          if (bytesRead < 2) {
            return KJ_EXCEPTION(DISCONNECTED, "WebSocket connection closed");
          }

          WebSocketFrame frame;
          frame.fin = (headerBuf[0] & 0x80) != 0;
          frame.opcode = static_cast<WebSocketOpcode>(headerBuf[0] & 0x0F);
          frame.masked = (headerBuf[1] & 0x80) != 0;

          uint64_t payload_len = headerBuf[1] & 0x7F;

          // Determine if we need extended length
          if (payload_len == 126) {
            // 16-bit extended length
            return read_extended_length_16(kj::mv(frame));
          } else if (payload_len == 127) {
            // 64-bit extended length
            return read_extended_length_64(kj::mv(frame));
          } else {
            return read_frame_payload(kj::mv(frame), payload_len);
          }
        });
  } else {
    return KJ_EXCEPTION(DISCONNECTED, "WebSocket not connected");
  }
}

kj::Promise<WebSocketFrame> BinanceWebSocket::read_extended_length_16(WebSocketFrame frame) {
  KJ_IF_SOME(streamOwn, stream_) {
    auto& stream = *streamOwn;
    auto extBuf = kj::heapArray<kj::byte>(2);
    return stream.read(extBuf.asPtr(), 2)
        .then([this, frame = kj::mv(frame),
               extBuf = kj::mv(extBuf)](size_t bytesRead) mutable -> kj::Promise<WebSocketFrame> {
          if (bytesRead < 2) {
            return KJ_EXCEPTION(DISCONNECTED,
                                "WebSocket connection closed reading extended length");
          }
          uint64_t len = (static_cast<uint64_t>(extBuf[0]) << 8) | static_cast<uint64_t>(extBuf[1]);
          return read_frame_payload(kj::mv(frame), len);
        });
  } else {
    return KJ_EXCEPTION(DISCONNECTED, "WebSocket not connected");
  }
}

kj::Promise<WebSocketFrame> BinanceWebSocket::read_extended_length_64(WebSocketFrame frame) {
  KJ_IF_SOME(streamOwn, stream_) {
    auto& stream = *streamOwn;
    auto extBuf = kj::heapArray<kj::byte>(8);
    return stream.read(extBuf.asPtr(), 8)
        .then([this, frame = kj::mv(frame),
               extBuf = kj::mv(extBuf)](size_t bytesRead) mutable -> kj::Promise<WebSocketFrame> {
          if (bytesRead < 8) {
            return KJ_EXCEPTION(DISCONNECTED,
                                "WebSocket connection closed reading extended length");
          }
          uint64_t len = 0;
          for (int i = 0; i < 8; ++i) {
            len = (len << 8) | static_cast<uint64_t>(extBuf[i]);
          }
          return read_frame_payload(kj::mv(frame), len);
        });
  } else {
    return KJ_EXCEPTION(DISCONNECTED, "WebSocket not connected");
  }
}

kj::Promise<WebSocketFrame> BinanceWebSocket::read_frame_payload(WebSocketFrame frame,
                                                                 uint64_t payload_len) {
  if (frame.masked) {
    // Read mask key first (4 bytes), then read payload with unmasking
    return read_mask_key(kj::mv(frame), payload_len);
  } else {
    // No mask, just read payload directly
    return read_unmasked_payload(kj::mv(frame), payload_len);
  }
}

kj::Promise<WebSocketFrame> BinanceWebSocket::read_mask_key(WebSocketFrame frame,
                                                            uint64_t payload_len) {
  KJ_IF_SOME(streamOwn, stream_) {
    auto& stream = *streamOwn;
    auto maskBuf = kj::heapArray<kj::byte>(4);
    return stream.read(maskBuf.asPtr(), 4)
        .then([this, frame = kj::mv(frame), maskBuf = kj::mv(maskBuf),
               payload_len](size_t bytesRead) mutable -> kj::Promise<WebSocketFrame> {
          if (bytesRead < 4) {
            return KJ_EXCEPTION(DISCONNECTED, "WebSocket connection closed reading mask key");
          }

          frame.mask_key = (static_cast<uint32_t>(maskBuf[0]) << 24) |
                           (static_cast<uint32_t>(maskBuf[1]) << 16) |
                           (static_cast<uint32_t>(maskBuf[2]) << 8) |
                           static_cast<uint32_t>(maskBuf[3]);

          if (payload_len > 0) {
            return read_masked_payload(kj::mv(frame), payload_len);
          } else {
            frame.payload = kj::heapArray<kj::byte>(0);
            return kj::mv(frame);
          }
        });
  } else {
    return KJ_EXCEPTION(DISCONNECTED, "WebSocket stream not available");
  }
}

kj::Promise<WebSocketFrame> BinanceWebSocket::read_masked_payload(WebSocketFrame frame,
                                                                  uint64_t payload_len) {
  KJ_IF_SOME(streamOwn, stream_) {
    auto& stream = *streamOwn;
    auto payloadBuf = kj::heapArray<kj::byte>(payload_len);
    return stream.read(payloadBuf.asPtr(), payload_len)
        .then([frame = kj::mv(frame), payloadBuf = kj::mv(payloadBuf)](
                  size_t bytesRead) mutable -> kj::Promise<WebSocketFrame> {
          if (bytesRead < payloadBuf.size()) {
            return KJ_EXCEPTION(DISCONNECTED, "WebSocket connection closed reading payload");
          }
          // Unmask payload
          for (size_t i = 0; i < payloadBuf.size(); ++i) {
            uint8_t mask_byte =
                static_cast<uint8_t>((frame.mask_key >> (8 * (3 - (i % 4)))) & 0xFF);
            payloadBuf[i] = static_cast<kj::byte>(static_cast<uint8_t>(payloadBuf[i]) ^ mask_byte);
          }
          frame.payload = kj::mv(payloadBuf);
          return kj::mv(frame);
        });
  } else {
    return KJ_EXCEPTION(DISCONNECTED, "WebSocket stream not available");
  }
}

kj::Promise<WebSocketFrame> BinanceWebSocket::read_unmasked_payload(WebSocketFrame frame,
                                                                    uint64_t payload_len) {
  if (payload_len == 0) {
    frame.payload = kj::heapArray<kj::byte>(0);
    return kj::mv(frame);
  }

  KJ_IF_SOME(streamOwn, stream_) {
    auto& stream = *streamOwn;
    auto payloadBuf = kj::heapArray<kj::byte>(payload_len);
    return stream.read(payloadBuf.asPtr(), payload_len)
        .then([frame = kj::mv(frame), payloadBuf = kj::mv(payloadBuf)](
                  size_t bytesRead) mutable -> kj::Promise<WebSocketFrame> {
          if (bytesRead < payloadBuf.size()) {
            return KJ_EXCEPTION(DISCONNECTED, "WebSocket connection closed reading payload");
          }
          frame.payload = kj::mv(payloadBuf);
          return kj::mv(frame);
        });
  } else {
    return KJ_EXCEPTION(DISCONNECTED, "WebSocket stream not available");
  }
}

kj::Promise<void> BinanceWebSocket::send_websocket_frame(kj::ArrayPtr<const kj::byte> payload,
                                                         WebSocketOpcode opcode) {
  KJ_IF_SOME(streamOwn, stream_) {
    auto& stream = *streamOwn;
    auto frame = encode_websocket_frame(payload, opcode, true); // Client frames must be masked
    return stream.write(frame.asPtr()).then([frame = kj::mv(frame)]() {
      // Frame sent successfully
    });
  } else {
    return KJ_EXCEPTION(DISCONNECTED, "WebSocket not connected");
  }
}

kj::Promise<void> BinanceWebSocket::send_text(kj::StringPtr message) {
  auto bytes = kj::arrayPtr(reinterpret_cast<const kj::byte*>(message.begin()), message.size());
  return send_websocket_frame(bytes, WebSocketOpcode::Text);
}

kj::Promise<void> BinanceWebSocket::send_ping() {
  return send_websocket_frame(kj::ArrayPtr<const kj::byte>(), WebSocketOpcode::Ping);
}

kj::Promise<void> BinanceWebSocket::send_pong(kj::ArrayPtr<const kj::byte> payload) {
  return send_websocket_frame(payload, WebSocketOpcode::Pong);
}

kj::Promise<void> BinanceWebSocket::connect() {
  if (connected_) {
    return kj::READY_NOW;
  }

  KJ_LOG(INFO, "Connecting to Binance WebSocket...", host_, port_);

  // Parse address and connect
  auto& network = ioContext_.provider->getNetwork();
  return network.parseAddress(host_, port_)
      .then([this](kj::Own<kj::NetworkAddress> addr) -> kj::Promise<kj::Own<kj::AsyncIoStream>> {
        return addr->connect();
      })
      .then(
          [this](kj::Own<kj::AsyncIoStream> tcpStream) -> kj::Promise<kj::Own<kj::AsyncIoStream>> {
            // Wrap with TLS
            return tlsContext_->wrapClient(kj::mv(tcpStream), host_);
          })
      .then([this](kj::Own<kj::AsyncIoStream> tlsStream) -> kj::Promise<void> {
        stream_ = kj::mv(tlsStream);

        // Send WebSocket handshake - need to access stream via KJ_IF_SOME
        KJ_IF_SOME(streamOwn, stream_) {
          auto& stream = *streamOwn;
          kj::String handshake = build_websocket_handshake();
          return stream.write(handshake.asBytes())
              .then([this, handshake = kj::mv(handshake)]() -> kj::Promise<void> {
                // Read handshake response
                KJ_IF_SOME(streamOwn2, stream_) {
                  auto& stream2 = *streamOwn2;
                  auto responseBuf = kj::heapArray<char>(4096);
                  return stream2.tryRead(responseBuf.begin(), 1, responseBuf.size())
                      .then([this,
                             responseBuf = kj::mv(responseBuf)](size_t bytesRead) mutable -> void {
                        if (bytesRead == 0) {
                          KJ_FAIL_REQUIRE("WebSocket handshake failed: no response");
                        }

                        kj::String response = kj::heapString(responseBuf.begin(), bytesRead);
                        kj::String expectedAccept = compute_accept_key(websocket_key_);

                        if (!validate_handshake_response(response, expectedAccept)) {
                          KJ_FAIL_REQUIRE("WebSocket handshake validation failed");
                        }

                        connected_ = true;
                        reset_reconnect_state();
                        KJ_LOG(INFO, "WebSocket connected successfully");
                      });
                } else {
                  KJ_FAIL_REQUIRE("Stream became unavailable during handshake");
                }
              });
        } else {
          KJ_FAIL_REQUIRE("Stream became unavailable after TLS wrap");
        }
      });
}

kj::Promise<void> BinanceWebSocket::disconnect() {
  if (!connected_) {
    return kj::READY_NOW;
  }

  KJ_LOG(INFO, "Disconnecting from Binance WebSocket...");

  // Send close frame if stream is available
  KJ_IF_SOME(streamOwn, stream_) {
    // Close frame with status code 1000 (normal closure)
    auto closePayload = kj::heapArray<kj::byte>(2);
    closePayload[0] = 0x03; // 1000 >> 8
    closePayload[1] = 0xE8; // 1000 & 0xFF

    return send_websocket_frame(closePayload, WebSocketOpcode::Close).then([this]() {
      connected_ = false;
      stream_ = kj::none;
      KJ_LOG(INFO, "WebSocket disconnected");
    });
  }

  connected_ = false;
  stream_ = kj::none;
  return kj::READY_NOW;
}

bool BinanceWebSocket::is_connected() const {
  return connected_.load();
}

kj::Promise<void> BinanceWebSocket::read_loop() {
  if (!running_ || !connected_) {
    return kj::READY_NOW;
  }

  return read_websocket_frame().then(
      [this](WebSocketFrame frame) -> kj::Promise<void> {
        switch (frame.opcode) {
        case WebSocketOpcode::Text:
        case WebSocketOpcode::Binary:
          if (frame.fin) {
            // Complete message
            if (fragment_buffer_.size() > 0) {
              // Append to fragment buffer and process
              for (auto b : frame.payload) {
                fragment_buffer_.add(b);
              }
              kj::String message = kj::heapString(reinterpret_cast<char*>(fragment_buffer_.begin()),
                                                  fragment_buffer_.size());
              handle_message(message);
              fragment_buffer_.clear();
            } else {
              // Single-frame message
              kj::String message = kj::heapString(reinterpret_cast<char*>(frame.payload.begin()),
                                                  frame.payload.size());
              handle_message(message);
            }
          } else {
            // Start of fragmented message
            fragment_opcode_ = frame.opcode;
            fragment_buffer_.clear();
            for (auto b : frame.payload) {
              fragment_buffer_.add(b);
            }
          }
          break;

        case WebSocketOpcode::Continuation:
          // Continue fragmented message
          for (auto b : frame.payload) {
            fragment_buffer_.add(b);
          }
          if (frame.fin) {
            // End of fragmented message
            kj::String message = kj::heapString(reinterpret_cast<char*>(fragment_buffer_.begin()),
                                                fragment_buffer_.size());
            handle_message(message);
            fragment_buffer_.clear();
          }
          break;

        case WebSocketOpcode::Ping:
          // Respond with pong
          return send_pong(frame.payload).then([this]() -> kj::Promise<void> {
            return read_loop();
          });

        case WebSocketOpcode::Pong:
          // Pong received, ignore
          break;

        case WebSocketOpcode::Close:
          KJ_LOG(INFO, "Received WebSocket close frame");
          connected_ = false;
          if (running_) {
            return schedule_reconnect();
          }
          return kj::READY_NOW;
        }

        // Continue reading
        return read_loop();
      },
      [this](kj::Exception&& e) -> kj::Promise<void> {
        KJ_LOG(ERROR, "WebSocket read error", e.getDescription());
        connected_ = false;
        if (running_) {
          return schedule_reconnect();
        }
        return kj::READY_NOW;
      });
}

kj::Promise<void> BinanceWebSocket::run() {
  running_ = true;

  if (!connected_) {
    return connect().then([this]() -> kj::Promise<void> {
      return resubscribe_all().then([this]() -> kj::Promise<void> { return read_loop(); });
    });
  }

  return read_loop();
}

void BinanceWebSocket::stop() {
  running_ = false;
}

kj::String BinanceWebSocket::format_symbol(const veloz::common::SymbolId& symbol) {
  // Convert symbol to lowercase using KJ
  kj::Vector<char> formatted;
  for (char c : symbol.value) {
    formatted.add((c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c);
  }
  formatted.add('\0');
  return kj::heapString(formatted.begin(), formatted.size() - 1);
}

kj::StringPtr BinanceWebSocket::event_type_to_stream_name(MarketEventType event_type) {
  switch (event_type) {
  case MarketEventType::Trade:
    return "@trade";
  case MarketEventType::BookTop:
    return "@bookTicker";
  case MarketEventType::BookDelta:
    return "@depth";
  case MarketEventType::Kline:
    return "@kline_1m";
  case MarketEventType::Ticker:
    return "@miniTicker";
  default:
    return "@trade";
  }
}

MarketEventType BinanceWebSocket::parse_stream_name(kj::StringPtr stream_name) {
  // Helper lambda to check if stream_name contains a substring
  auto contains = [&stream_name](kj::StringPtr needle) -> bool {
    if (needle.size() > stream_name.size()) return false;
    for (size_t i = 0; i <= stream_name.size() - needle.size(); ++i) {
      bool match = true;
      for (size_t j = 0; j < needle.size(); ++j) {
        if (stream_name[i + j] != needle[j]) {
          match = false;
          break;
        }
      }
      if (match) return true;
    }
    return false;
  };

  if (contains("@trade"_kj)) {
    return MarketEventType::Trade;
  } else if (contains("@bookTicker"_kj)) {
    return MarketEventType::BookTop;
  } else if (contains("@depth"_kj)) {
    return MarketEventType::BookDelta;
  } else if (contains("@kline"_kj)) {
    return MarketEventType::Kline;
  } else if (contains("@miniTicker"_kj)) {
    return MarketEventType::Ticker;
  }

  return MarketEventType::Unknown;
}

kj::String BinanceWebSocket::build_subscription_message(const veloz::common::SymbolId& symbol,
                                                        MarketEventType event_type,
                                                        bool subscribe) {
  kj::String formatted_symbol = format_symbol(symbol);
  kj::StringPtr stream_suffix = event_type_to_stream_name(event_type);
  kj::String stream = kj::str(formatted_symbol, stream_suffix);

  auto builder = JsonBuilder::object();
  builder.put("method", subscribe ? "SUBSCRIBE" : "UNSUBSCRIBE");
  builder.put_array("params", [&](JsonBuilder& arr) { arr.add(std::string(stream.cStr())); });
  builder.put("id", subscription_state_.lockExclusive()->next_subscription_id++);

  return kj::str(builder.build().c_str());
}

kj::Promise<bool> BinanceWebSocket::subscribe(const veloz::common::SymbolId& symbol,
                                              MarketEventType event_type) {
  {
    auto lock = subscription_state_.lockExclusive();
    kj::String symbol_key = kj::heapString(symbol.value);

    // Check if already subscribed
    auto find_result = lock->subscriptions.find(symbol_key);
    KJ_IF_SOME(event_types_ref, find_result) {
      for (size_t i = 0; i < event_types_ref.size(); ++i) {
        if (event_types_ref[i] == event_type) {
          return kj::Promise<bool>(true); // Already subscribed
        }
      }
      // Add event type to existing subscription
      event_types_ref.add(event_type);
    } else {
      // Create new subscription entry
      kj::Vector<MarketEventType> event_types;
      event_types.add(event_type);
      lock->subscriptions.insert(kj::mv(symbol_key), kj::mv(event_types));
    }
  }

  // Send subscription message if connected
  KJ_IF_SOME(streamOwn, stream_) {
    (void)streamOwn;
    if (connected_) {
      kj::String msg = build_subscription_message(symbol, event_type, true);
      KJ_LOG(INFO, "Subscribing to stream", msg);
      return send_text(msg).then([]() -> bool { return true; });
    }
  }

  return kj::Promise<bool>(true);
}

kj::Promise<bool> BinanceWebSocket::unsubscribe(const veloz::common::SymbolId& symbol,
                                                MarketEventType event_type) {
  {
    auto lock = subscription_state_.lockExclusive();
    kj::String symbol_key = kj::heapString(symbol.value);

    auto find_result = lock->subscriptions.find(symbol_key);
    KJ_IF_SOME(event_types_ref, find_result) {
      bool found = false;
      kj::Vector<MarketEventType> new_types;

      for (size_t i = 0; i < event_types_ref.size(); ++i) {
        if (event_types_ref[i] == event_type) {
          found = true;
        } else {
          new_types.add(event_types_ref[i]);
        }
      }

      if (!found) {
        return kj::Promise<bool>(false); // Not subscribed to this event type
      }

      if (new_types.size() == 0) {
        lock->subscriptions.erase(symbol_key);
      } else {
        // Replace with filtered list
        event_types_ref.clear();
        for (auto& t : new_types) {
          event_types_ref.add(t);
        }
      }
    } else {
      return kj::Promise<bool>(false); // Not subscribed
    }
  }

  // Send unsubscribe message if connected
  KJ_IF_SOME(streamOwn, stream_) {
    (void)streamOwn;
    if (connected_) {
      kj::String msg = build_subscription_message(symbol, event_type, false);
      KJ_LOG(INFO, "Unsubscribing from stream", msg);
      return send_text(msg).then([]() -> bool { return true; });
    }
  }

  return kj::Promise<bool>(true);
}

void BinanceWebSocket::set_event_callback(MarketEventCallback callback) {
  auto lock = callback_state_.lockExclusive();
  lock->callback = kj::mv(callback);
}

kj::Promise<void> BinanceWebSocket::resubscribe_all() {
  KJ_IF_SOME(streamOwn, stream_) {
    (void)streamOwn;
    kj::Vector<kj::Promise<void>> promises;

    auto lock = subscription_state_.lockExclusive();
    for (const auto& entry : lock->subscriptions) {
      kj::StringPtr symbol_key = entry.key;
      const kj::Vector<MarketEventType>& event_types = entry.value;

      // Reconstruct SymbolId from key
      veloz::common::SymbolId symbol{symbol_key};

      for (size_t i = 0; i < event_types.size(); ++i) {
        kj::String msg = build_subscription_message(symbol, event_types[i], true);
        KJ_LOG(INFO, "Resubscribing to stream", msg);
        promises.add(send_text(msg));
      }
    }

    if (promises.size() == 0) {
      return kj::READY_NOW;
    }

    return kj::joinPromises(promises.releaseAsArray());
  } else {
    return kj::READY_NOW;
  }
}

kj::Promise<void> BinanceWebSocket::schedule_reconnect() {
  if (!running_) {
    return kj::READY_NOW;
  }

  reconnect_attempts_++;
  reconnect_count_++;

  // Exponential backoff with jitter, capped at 30 seconds
  auto delay_ms = reconnect_delay_ms_.load();
  reconnect_delay_ms_.store(kj::min(delay_ms * 2, static_cast<std::int64_t>(30000)));

  // Add jitter (0-25%)
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> jitter(0, static_cast<int>(delay_ms / 4));
  delay_ms += jitter(gen);

  KJ_LOG(INFO, "Scheduling reconnection", delay_ms, "ms", "attempt", reconnect_attempts_.load());

  auto& timer = ioContext_.provider->getTimer();
  return timer.afterDelay(delay_ms * kj::MILLISECONDS).then([this]() -> kj::Promise<void> {
    if (running_) {
      return connect().then([this]() -> kj::Promise<void> {
        return resubscribe_all().then([this]() -> kj::Promise<void> { return read_loop(); });
      });
    }
    return kj::READY_NOW;
  });
}

void BinanceWebSocket::reset_reconnect_state() {
  reconnect_delay_ms_.store(1000);
  reconnect_attempts_.store(0);
}

void BinanceWebSocket::handle_message(kj::StringPtr message) {
  try {
    auto doc = JsonDocument::parse(std::string(message.cStr()));
    auto root = doc.root();

    auto stream = root["stream"];
    auto data = root["data"];

    if (!stream.is_string() || !data.is_valid()) {
      return;
    }

    kj::String stream_str = kj::heapString(stream.get_string().c_str());

    // Parse symbol and event type from stream name - find '@' separator
    kj::Maybe<size_t> separator_pos = kj::none;
    for (size_t i = 0; i < stream_str.size(); ++i) {
      if (stream_str[i] == '@') {
        separator_pos = i;
        break;
      }
    }

    KJ_IF_SOME(pos, separator_pos) {
      // Extract symbol and convert to uppercase
      kj::Vector<char> symbol_chars;
      for (size_t i = 0; i < pos; ++i) {
        char c = stream_str[i];
        symbol_chars.add((c >= 'a' && c <= 'z') ? (c - ('a' - 'A')) : c);
      }
      symbol_chars.add('\0');
      veloz::common::SymbolId symbol(kj::StringPtr(symbol_chars.begin()));

      MarketEventType event_type = parse_stream_name(stream_str);

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
        return;
      }

      // Call callback if set
      {
        auto lock = callback_state_.lockExclusive();
        KJ_IF_SOME(cb, lock->callback) {
          cb(market_event);
        }
      }

      message_count_++;
      last_message_time_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    }
  } catch (const std::exception& e) {
    KJ_LOG(ERROR, "Error parsing WebSocket message", e.what());
  }
}

MarketEvent BinanceWebSocket::parse_trade_message(const veloz::core::JsonValue& data,
                                                  const veloz::common::SymbolId& symbol) {
  MarketEvent event;
  event.type = MarketEventType::Trade;
  event.venue = veloz::common::Venue::Binance;
  event.symbol = symbol;
  event.market = veloz::common::MarketKind::Spot;

  event.ts_exchange_ns = data["T"].get_int(0LL) * 1000000;
  event.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();

  TradeData trade_data;
  trade_data.price = data["p"].get_double(0.0);
  trade_data.qty = data["q"].get_double(0.0);
  trade_data.is_buyer_maker = data["m"].get_bool(false);
  trade_data.trade_id = data["t"].get_int(0LL);

  event.data = kj::mv(trade_data);
  event.payload = kj::heapString("");

  return event;
}

MarketEvent BinanceWebSocket::parse_book_message(const veloz::core::JsonValue& data,
                                                 const veloz::common::SymbolId& symbol,
                                                 bool is_book_top) {
  MarketEvent event;
  event.type = is_book_top ? MarketEventType::BookTop : MarketEventType::BookDelta;
  event.venue = veloz::common::Venue::Binance;
  event.symbol = symbol;
  event.market = veloz::common::MarketKind::Spot;

  event.ts_exchange_ns = data["E"].get_int(0LL) * 1000000;
  event.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();

  BookData book_data;
  if (is_book_top) {
    BookLevel bid_level;
    bid_level.price = data["b"].get_double(0.0);
    bid_level.qty = data["B"].get_double(0.0);
    book_data.bids.add(bid_level);

    BookLevel ask_level;
    ask_level.price = data["a"].get_double(0.0);
    ask_level.qty = data["A"].get_double(0.0);
    book_data.asks.add(ask_level);

    book_data.sequence = data["u"].get_int(0LL);
  } else {
    auto bids = data["b"];
    if (bids.is_array()) {
      bids.for_each_array([&](const JsonValue& bid) {
        double price = std::stod(bid[0].get_string());
        double qty = std::stod(bid[1].get_string());
        BookLevel level;
        level.price = price;
        level.qty = qty;
        book_data.bids.add(level);
      });
    }

    auto asks = data["a"];
    if (asks.is_array()) {
      asks.for_each_array([&](const JsonValue& ask) {
        double price = std::stod(ask[0].get_string());
        double qty = std::stod(ask[1].get_string());
        BookLevel level;
        level.price = price;
        level.qty = qty;
        book_data.asks.add(level);
      });
    }

    book_data.sequence = data["u"].get_int(0LL);
  }

  event.data = kj::mv(book_data);
  event.payload = kj::heapString("");

  return event;
}

MarketEvent BinanceWebSocket::parse_kline_message(const veloz::core::JsonValue& data,
                                                  const veloz::common::SymbolId& symbol) {
  MarketEvent event;
  event.type = MarketEventType::Kline;
  event.venue = veloz::common::Venue::Binance;
  event.symbol = symbol;
  event.market = veloz::common::MarketKind::Spot;

  auto kline = data["k"];

  event.ts_exchange_ns = data["E"].get_int(0LL) * 1000000;
  event.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();

  KlineData kline_data;
  kline_data.open = std::stod(kline["o"].get_string());
  kline_data.high = std::stod(kline["h"].get_string());
  kline_data.low = std::stod(kline["l"].get_string());
  kline_data.close = std::stod(kline["c"].get_string());
  kline_data.volume = std::stod(kline["v"].get_string());
  kline_data.start_time = kline["t"].get_int(0LL);
  kline_data.close_time = kline["T"].get_int(0LL);

  event.data = kj::mv(kline_data);
  event.payload = kj::heapString("");

  return event;
}

MarketEvent BinanceWebSocket::parse_ticker_message(const veloz::core::JsonValue& data,
                                                   const veloz::common::SymbolId& symbol) {
  MarketEvent event;
  event.type = MarketEventType::Ticker;
  event.venue = veloz::common::Venue::Binance;
  event.symbol = symbol;
  event.market = veloz::common::MarketKind::Spot;

  event.ts_exchange_ns = data["E"].get_int(0LL) * 1000000;
  event.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();

  // TickerData not yet defined in market_event.h, use EmptyData for now
  event.data = EmptyData{};
  event.payload = kj::heapString("");

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
