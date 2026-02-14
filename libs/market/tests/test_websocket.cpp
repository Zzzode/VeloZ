// Copyright 2026 VeloZ Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <kj/array.h>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/encoding.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/test.h>

namespace {

// Test SHA-1 implementation for WebSocket handshake
// Simple SHA-1 implementation for testing (same as used in binance_websocket.cpp)
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

  void update(const void* data, size_t len) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    total_bits_ += len * 8;

    while (len > 0) {
      size_t to_copy = std::min(len, 64 - buffer_size_);
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

// WebSocket magic GUID for Sec-WebSocket-Accept computation (RFC 6455)
constexpr const char* WS_MAGIC_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// Test opcode values
enum class WebSocketOpcode : std::uint8_t {
  Continuation = 0x0,
  Text = 0x1,
  Binary = 0x2,
  Close = 0x8,
  Ping = 0x9,
  Pong = 0xA
};

// Encode a WebSocket frame (same as used in binance_websocket.cpp)
kj::Array<kj::byte> encode_websocket_frame(kj::ArrayPtr<const kj::byte> payload,
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

  // Simple mask for testing (use static key for reproducibility)
  if (mask) {
    uint32_t mask_key = 0x12345678;
    frame[offset++] = static_cast<kj::byte>((mask_key >> 24) & 0xFF);
    frame[offset++] = static_cast<kj::byte>((mask_key >> 16) & 0xFF);
    frame[offset++] = static_cast<kj::byte>((mask_key >> 8) & 0xFF);
    frame[offset++] = static_cast<kj::byte>(mask_key & 0xFF);

    for (size_t i = 0; i < payload_len; ++i) {
      uint8_t mask_byte = static_cast<uint8_t>((mask_key >> (8 * (3 - (i % 4)))) & 0xFF);
      frame[offset + i] = static_cast<kj::byte>(static_cast<uint8_t>(payload[i]) ^ mask_byte);
    }
  } else {
    std::memcpy(frame.begin() + offset, payload.begin(), payload_len);
  }

  return frame;
}

// KJ Test Framework Test Suites
// ============================================================================

KJ_TEST("SHA1: EmptyString") {
  SHA1 sha1;
  sha1.update("", 0);
  auto hash = sha1.finalize();
  kj::String hashHex = kj::encodeHex(hash);
  // SHA-1 of empty string is: da39a3ee5e6b4b0d3255bfef95601890afd80709
  KJ_ASSERT(hashHex == kj::heapString("da39a3ee5e6b4b0d3255bfef95601890afd80709"), hashHex);
}

KJ_TEST("SHA1: KnownTestVector1") {
  SHA1 sha1;
  sha1.update("abc", 3);
  auto hash = sha1.finalize();
  kj::String hashHex = kj::encodeHex(hash);
  // SHA-1 of "abc": a9993e364706816aba3e25717850c26c9cd0d89d
  KJ_ASSERT(hashHex == kj::heapString("a9993e364706816aba3e25717850c26c9cd0d89d"), hashHex);
}

KJ_TEST("SHA1: KnownTestVector2") {
  SHA1 sha1;
  const char* msg = "The quick brown fox jumps over the lazy dog";
  sha1.update(msg, strlen(msg));
  auto hash = sha1.finalize();
  kj::String hashHex = kj::encodeHex(hash);
  // 2fd4e1c67a2d28fced849ee1bb76e7391b93eb12
  KJ_ASSERT(hashHex == kj::heapString("2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"), hashHex);
}

KJ_TEST("SHA1: LongString") {
  SHA1 sha1;
  std::string input;
  input.resize(1000000, 'a');
  sha1.update(input.data(), input.size());
  auto hash = sha1.finalize();
  kj::String hashHex = kj::encodeHex(hash);
  // Known SHA-1 for million 'a's
  KJ_ASSERT(hashHex == kj::heapString("34aa973cd4c4daa4f61eeb2bdbad27316534016f"), hashHex);
}

// ============================================================================
// WebSocket Handshake Tests
// ============================================================================

KJ_TEST("WebSocketHandshake: KnownKeyAccept") {
  // Test with known key: dGhlIHNhbXBsZSBub25jZQ==
  // Expected accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
  kj::String key = kj::heapString("dGhlIHNhbXBsZSBub25jZQ==");
  kj::String combined = kj::str(key, kj::heapString(WS_MAGIC_GUID));

  SHA1 sha1;
  sha1.update(combined.cStr(), combined.size());
  auto hash = sha1.finalize();
  kj::String accept = kj::encodeBase64(hash);

  KJ_ASSERT(accept == kj::heapString("s3pPLMBiTxaQ9kYGzzhZRbK+xOo="), accept);
}

KJ_TEST("WebSocketHandshake: DifferentKey") {
  // Test with another known key
  kj::String key = kj::heapString("AQIDBAUGBwgJCgsMDQ4PEA==");
  kj::String combined = kj::str(key, kj::heapString(WS_MAGIC_GUID));

  SHA1 sha1;
  sha1.update(combined.cStr(), combined.size());
  auto hash = sha1.finalize();
  kj::String accept = kj::encodeBase64(hash);

  // Just verify it's a valid base64 string (44 chars for 32 bytes SHA-1)
  KJ_ASSERT(accept.size() == 28);
}

// ============================================================================
// WebSocket Frame Encoding Tests
// ============================================================================

KJ_TEST("WebSocketFrame: ShortUnmaskedTextFrame") {
  kj::String payload = kj::heapString("Hello");
  auto frame = encode_websocket_frame(
      kj::arrayPtr(reinterpret_cast<const kj::byte*>(payload.cStr()), payload.size()),
      WebSocketOpcode::Text, false);

  // Verify header structure
  KJ_ASSERT((frame[0] & 0x80) == 0x80); // FIN bit set
  KJ_ASSERT((frame[0] & 0x0F) == 0x01); // Opcode = Text
  KJ_ASSERT((frame[1] & 0x80) == 0x00); // No masking
  KJ_ASSERT((frame[1] & 0x7F) == 0x05); // Length = 5

  // Verify payload
  KJ_ASSERT(kj::StringPtr(reinterpret_cast<char*>(frame.begin() + 2), 5) == kj::StringPtr("Hello"));
}

KJ_TEST("WebSocketFrame: ShortMaskedTextFrame") {
  kj::String payload = kj::heapString("Hello");
  auto frame = encode_websocket_frame(
      kj::arrayPtr(reinterpret_cast<const kj::byte*>(payload.cStr()), payload.size()),
      WebSocketOpcode::Text, true);

  // Verify header structure
  KJ_ASSERT((frame[0] & 0x80) == 0x80); // FIN bit set
  KJ_ASSERT((frame[0] & 0x0F) == 0x01); // Opcode = Text
  KJ_ASSERT((frame[1] & 0x80) == 0x80); // Masking set
  KJ_ASSERT((frame[1] & 0x7F) == 0x05); // Length = 5

  // Verify mask key is present (bytes 2-5)
  KJ_ASSERT(frame.size() == 11); // 2 bytes header + 4 mask + 5 payload
}

KJ_TEST("WebSocketFrame: MediumLengthFrame") {
  // Test payload of 200 bytes (needs 2-byte extended length)
  kj::Array<kj::byte> payload = kj::heapArray<kj::byte>(200);
  for (size_t i = 0; i < 200; ++i) {
    payload[i] = static_cast<kj::byte>(i);
  }

  auto frame = encode_websocket_frame(payload, WebSocketOpcode::Binary, false);

  // Verify header uses 126 for extended length
  KJ_ASSERT((frame[0] & 0x0F) == 0x02); // Opcode = Binary
  KJ_ASSERT((frame[1] & 0x7F) == 126);  // Extended length marker

  // Verify extended length field
  uint16_t extended_len = (static_cast<uint16_t>(frame[2]) << 8) | static_cast<uint16_t>(frame[3]);
  KJ_ASSERT(extended_len == 200);

  KJ_ASSERT(frame.size() == 204); // 2 + 2 + 200
}

KJ_TEST("WebSocketFrame: LargeLengthFrame") {
  // Test payload of 100000 bytes (needs 8-byte extended length)
  kj::Array<kj::byte> payload = kj::heapArray<kj::byte>(100000);
  for (size_t i = 0; i < 100000; ++i) {
    payload[i] = static_cast<kj::byte>(i & 0xFF);
  }

  auto frame = encode_websocket_frame(payload, WebSocketOpcode::Binary, false);

  // Verify header uses 127 for extended length
  KJ_ASSERT((frame[1] & 0x7F) == 127); // Extended length marker

  // Verify extended length field (8 bytes, big-endian)
  uint64_t extended_len = 0;
  for (int i = 0; i < 8; ++i) {
    extended_len = (extended_len << 8) | static_cast<uint64_t>(frame[2 + i]);
  }
  KJ_ASSERT(extended_len == 100000);

  KJ_ASSERT(frame.size() == 100010); // 2 + 8 + 100000
}

KJ_TEST("WebSocketFrame: CloseFrame") {
  // Close frame with status code 1000 (normal closure)
  kj::Array<kj::byte> payload = kj::heapArray<kj::byte>(2);
  payload[0] = 0x03; // 1000 >> 8
  payload[1] = 0xE8; // 1000 & 0xFF

  auto frame = encode_websocket_frame(payload, WebSocketOpcode::Close, true);

  KJ_ASSERT((frame[0] & 0x0F) == 0x08); // Opcode = Close
  KJ_ASSERT((frame[0] & 0x80) == 0x80); // FIN set
  KJ_ASSERT(frame.size() == 8);         // 2 header + 4 mask + 2 payload
}

KJ_TEST("WebSocketFrame: PingFrame") {
  // Ping frame (no payload)
  auto frame = encode_websocket_frame(kj::ArrayPtr<const kj::byte>(), WebSocketOpcode::Ping, true);

  KJ_ASSERT((frame[0] & 0x0F) == 0x09); // Opcode = Ping
  KJ_ASSERT((frame[0] & 0x80) == 0x80); // FIN set
  KJ_ASSERT((frame[1] & 0x7F) == 0x00); // Length = 0
  KJ_ASSERT(frame.size() == 6);         // 2 header + 4 mask + 0 payload
}

KJ_TEST("WebSocketFrame: PongFrame") {
  // Pong frame with payload
  kj::Array<kj::byte> payload = kj::heapArray<kj::byte>(4);
  payload[0] = 0x11;
  payload[1] = 0x22;
  payload[2] = 0x33;
  payload[3] = 0x44;

  auto frame = encode_websocket_frame(payload, WebSocketOpcode::Pong, true);

  KJ_ASSERT((frame[0] & 0x0F) == 0x0A); // Opcode = Pong
  KJ_ASSERT((frame[0] & 0x80) == 0x80); // FIN set
  KJ_ASSERT(frame.size() == 10);        // 2 header + 4 mask + 4 payload
}

// ============================================================================
// WebSocket Opcode Tests
// ============================================================================

KJ_TEST("WebSocketOpcode: ContinuationOpcode") {
  auto frame =
      encode_websocket_frame(kj::ArrayPtr<const kj::byte>(), WebSocketOpcode::Continuation, false);
  KJ_ASSERT((frame[0] & 0x0F) == 0x00);
}

KJ_TEST("WebSocketOpcode: TextOpcode") {
  auto frame = encode_websocket_frame(kj::ArrayPtr<const kj::byte>(), WebSocketOpcode::Text, false);
  KJ_ASSERT((frame[0] & 0x0F) == 0x01);
}

KJ_TEST("WebSocketOpcode: BinaryOpcode") {
  auto frame =
      encode_websocket_frame(kj::ArrayPtr<const kj::byte>(), WebSocketOpcode::Binary, false);
  KJ_ASSERT((frame[0] & 0x0F) == 0x02);
}

KJ_TEST("WebSocketOpcode: CloseOpcode") {
  auto frame =
      encode_websocket_frame(kj::ArrayPtr<const kj::byte>(), WebSocketOpcode::Close, false);
  KJ_ASSERT((frame[0] & 0x0F) == 0x08);
}

KJ_TEST("WebSocketOpcode: PingOpcode") {
  auto frame = encode_websocket_frame(kj::ArrayPtr<const kj::byte>(), WebSocketOpcode::Ping, false);
  KJ_ASSERT((frame[0] & 0x0F) == 0x09);
}

KJ_TEST("WebSocketOpcode: PongOpcode") {
  auto frame = encode_websocket_frame(kj::ArrayPtr<const kj::byte>(), WebSocketOpcode::Pong, false);
  KJ_ASSERT((frame[0] & 0x0F) == 0x0A);
}

// ============================================================================
// WebSocket Masking Tests
// ============================================================================

KJ_TEST("WebSocketMasking: MaskedPayloadNotPlaintext") {
  kj::String payload = kj::heapString("Hello, WebSocket!");
  auto frame = encode_websocket_frame(
      kj::arrayPtr(reinterpret_cast<const kj::byte*>(payload.cStr()), payload.size()),
      WebSocketOpcode::Text, true);

  // Payload should be masked (not equal to original)
  auto payloadStart = frame.begin() + 6; // Skip 2 header + 4 mask bytes
  auto maskedPayload = kj::arrayPtr(payloadStart, payload.size());
  bool isDifferent = false;
  for (size_t i = 0; i < payload.size(); ++i) {
    if (static_cast<char>(maskedPayload[i]) != payload.cStr()[i]) {
      isDifferent = true;
      break;
    }
  }
  KJ_ASSERT(isDifferent);
}

KJ_TEST("WebSocketMasking: UnmaskedPayloadIsPlaintext") {
  kj::String payload = kj::heapString("Hello, WebSocket!");
  auto frame = encode_websocket_frame(
      kj::arrayPtr(reinterpret_cast<const kj::byte*>(payload.cStr()), payload.size()),
      WebSocketOpcode::Text, false);

  // Payload should be unmasked (equal to original)
  auto payloadStart = frame.begin() + 2; // Skip 2 header bytes
  auto unmaskedPayload = kj::arrayPtr(payloadStart, payload.size());
  bool isEqual = true;
  for (size_t i = 0; i < payload.size(); ++i) {
    if (static_cast<char>(unmaskedPayload[i]) != payload.cStr()[i]) {
      isEqual = false;
      break;
    }
  }
  KJ_ASSERT(isEqual);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

KJ_TEST("WebSocketEdgeCase: EmptyPayload") {
  auto frame = encode_websocket_frame(kj::ArrayPtr<const kj::byte>(), WebSocketOpcode::Text, false);

  KJ_ASSERT((frame[0] & 0x0F) == 0x01); // Opcode = Text
  KJ_ASSERT((frame[1] & 0x7F) == 0x00); // Length = 0
  KJ_ASSERT(frame.size() == 2);         // Only header bytes
}

KJ_TEST("WebSocketEdgeCase: MaximumSmallPayload") {
  // Test payload of exactly 125 bytes
  kj::Array<kj::byte> payload = kj::heapArray<kj::byte>(125);
  for (size_t i = 0; i < 125; ++i) {
    payload[i] = static_cast<kj::byte>(i);
  }

  auto frame = encode_websocket_frame(payload, WebSocketOpcode::Binary, false);

  KJ_ASSERT((frame[1] & 0x7F) == 125); // Uses 7-bit length
  KJ_ASSERT(frame.size() == 127);      // 2 + 125
}

KJ_TEST("WebSocketEdgeCase: MinimumExtendedLength") {
  // Test payload of exactly 126 bytes (smallest for 16-bit extended length)
  kj::Array<kj::byte> payload = kj::heapArray<kj::byte>(126);
  for (size_t i = 0; i < 126; ++i) {
    payload[i] = static_cast<kj::byte>(i);
  }

  auto frame = encode_websocket_frame(payload, WebSocketOpcode::Binary, false);

  KJ_ASSERT((frame[1] & 0x7F) == 126); // Uses extended length marker
  KJ_ASSERT(frame.size() == 130);      // 2 + 2 + 126
}

KJ_TEST("WebSocketEdgeCase: Maximum16BitLength") {
  // Test payload of exactly 65535 bytes (maximum for 16-bit)
  kj::Array<kj::byte> payload = kj::heapArray<kj::byte>(65535);
  for (size_t i = 0; i < 65535; ++i) {
    payload[i] = static_cast<kj::byte>(i & 0xFF);
  }

  auto frame = encode_websocket_frame(payload, WebSocketOpcode::Binary, false);

  KJ_ASSERT((frame[1] & 0x7F) == 126); // Uses 16-bit extended length
  KJ_ASSERT(frame.size() == 65539);    // 2 + 2 + 65535
}

KJ_TEST("WebSocketEdgeCase: Minimum64BitLength") {
  // Test payload of exactly 65536 bytes (minimum for 64-bit)
  kj::Array<kj::byte> payload = kj::heapArray<kj::byte>(65536);
  for (size_t i = 0; i < 65536; ++i) {
    payload[i] = static_cast<kj::byte>(i & 0xFF);
  }

  auto frame = encode_websocket_frame(payload, WebSocketOpcode::Binary, false);

  KJ_ASSERT((frame[1] & 0x7F) == 127); // Uses 64-bit extended length
  KJ_ASSERT(frame.size() == 65546);    // 2 + 8 + 65536
}

// ============================================================================
// Base64 Encoding Tests
// ============================================================================

KJ_TEST("Base64: SimpleString") {
  kj::String input = kj::heapString("Hello");
  auto bytes = kj::arrayPtr(reinterpret_cast<const kj::byte*>(input.cStr()), input.size());
  auto encoded = kj::encodeBase64(bytes);
  // "Hello" in base64 is "SGVsbG8="
  KJ_ASSERT(encoded == kj::heapString("SGVsbG8="));
}

KJ_TEST("Base64: WebSocketKey") {
  // Test known WebSocket key encoding
  kj::String input = kj::heapString("dGhlIHNhbXBsZSBub25jZQ==");
  auto decoded = kj::decodeBase64(input);
  kj::String reEncoded = kj::encodeBase64(decoded);
  KJ_ASSERT(reEncoded == input);
}

} // namespace
