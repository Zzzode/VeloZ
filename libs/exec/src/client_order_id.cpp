#include "veloz/exec/client_order_id.h"

#include <chrono>
#include <kj/array.h>
#include <random>

namespace veloz::exec {

ClientOrderIdGenerator::ClientOrderIdGenerator(kj::StringPtr strategy_id)
    : strategy_id_(kj::str(strategy_id)) {}

kj::String ClientOrderIdGenerator::generate() {
  // Get current timestamp in seconds using std::chrono
  // Note: KJ's time types are for async I/O timing, not wall clock time.
  // std::chrono is used here because we need Unix epoch timestamps for order IDs.
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

  ++sequence_;

  // Generate random component for uniqueness across processes
  // Note: std::random_device and std::mt19937 are used here because KJ does not
  // provide a random number generator. This is acceptable per CLAUDE.md guidelines.
  static thread_local std::random_device rd;
  static thread_local std::mt19937 gen(rd());
  static thread_local std::uniform_int_distribution<> dis(0, 15);

  // Build hex string using KJ
  auto hex_builder = kj::heapArrayBuilder<char>(4);
  static constexpr char hex_chars[] = "0123456789abcdef";
  for (int i = 0; i < 4; ++i) {
    hex_builder.add(hex_chars[dis(gen)]);
  }
  auto hex_array = hex_builder.finish();
  kj::String hex_str = kj::str(kj::StringPtr(hex_array.begin(), hex_array.size()));

  return kj::str(strategy_id_, "-", timestamp, "-", sequence_, "-", hex_str);
}

ParsedClientOrderId ClientOrderIdGenerator::parse(kj::StringPtr client_order_id) {
  // Format: STRATEGY-TIMESTAMP-SEQUENCE-RANDOM
  // Returns: ParsedClientOrderId with strategy, timestamp, and unique (sequence-random or empty)

  ParsedClientOrderId result;
  result.timestamp = 0;

  // Find first dash
  kj::Maybe<size_t> first_dash;
  for (size_t i = 0; i < client_order_id.size(); ++i) {
    if (client_order_id[i] == '-') {
      first_dash = i;
      break;
    }
  }

  KJ_IF_SOME(fd, first_dash) {
    // Find second dash
    kj::Maybe<size_t> second_dash;
    for (size_t i = fd + 1; i < client_order_id.size(); ++i) {
      if (client_order_id[i] == '-') {
        second_dash = i;
        break;
      }
    }

    KJ_IF_SOME(sd, second_dash) {
      // Extract strategy
      result.strategy = kj::str(client_order_id.slice(0, fd));

      // Extract and parse timestamp
      auto timestamp_str = client_order_id.slice(fd + 1, sd);
      // Parse timestamp manually
      std::int64_t ts = 0;
      for (size_t i = 0; i < timestamp_str.size(); ++i) {
        char c = timestamp_str[i];
        if (c >= '0' && c <= '9') {
          ts = ts * 10 + (c - '0');
        }
      }
      result.timestamp = ts;

      // Find third dash for unique component
      kj::Maybe<size_t> third_dash;
      for (size_t i = sd + 1; i < client_order_id.size(); ++i) {
        if (client_order_id[i] == '-') {
          third_dash = i;
          break;
        }
      }

      KJ_IF_SOME(td, third_dash) {
        // Has both sequence and random parts
        result.unique = kj::str(client_order_id.slice(sd + 1));
      }
      else {
        // No random part - return empty unique per test expectation
        result.unique = kj::str("");
      }
    }
    else {
      // Invalid format - return empty result
      result.strategy = kj::str("");
      result.unique = kj::str("");
    }
  }
  else {
    // Invalid format - return empty result
    result.strategy = kj::str("");
    result.unique = kj::str("");
  }

  return result;
}

} // namespace veloz::exec
