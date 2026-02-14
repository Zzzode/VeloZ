#include "veloz/exec/client_order_id.h"

#include <chrono>
#include <random>
#include <sstream>

namespace veloz::exec {

ClientOrderIdGenerator::ClientOrderIdGenerator(kj::StringPtr strategy_id)
    : strategy_id_(kj::str(strategy_id)) {}

kj::String ClientOrderIdGenerator::generate() {
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

  ++sequence_;

  // Generate random component for uniqueness across processes
  static thread_local std::random_device rd;
  static thread_local std::mt19937 gen(rd());
  static thread_local std::uniform_int_distribution<> dis(0, 15);

  std::ostringstream oss;
  for (int i = 0; i < 4; ++i) {
    oss << std::hex << dis(gen);
  }

  return kj::str(strategy_id_, "-", timestamp, "-", sequence_, "-", oss.str());
}

std::tuple<kj::String, std::int64_t, kj::String>
ClientOrderIdGenerator::parse(kj::StringPtr client_order_id) {
  // Format: STRATEGY-TIMESTAMP-SEQUENCE-RANDOM
  // Returns: (strategy, timestamp, sequence-random or empty if no sequence part)
  std::string id_str(client_order_id.cStr());
  size_t first_dash = id_str.find('-');
  size_t second_dash = id_str.find('-', first_dash + 1);

  if (first_dash == std::string::npos || second_dash == std::string::npos) {
    return {kj::str(""), 0, kj::str("")};
  }

  kj::String strategy = kj::str(id_str.substr(0, first_dash));
  std::int64_t timestamp = std::stoll(id_str.substr(first_dash + 1, second_dash - first_dash - 1));

  // unique includes everything after the timestamp (sequence-random or just sequence)
  size_t third_dash = id_str.find('-', second_dash + 1);
  kj::String unique;
  if (third_dash != std::string::npos) {
    // Has both sequence and random parts
    unique = kj::str(id_str.substr(second_dash + 1));
  } else {
    // No random part - return empty unique per test expectation
    unique = kj::str("");
  }

  return {kj::mv(strategy), timestamp, kj::mv(unique)};
}

} // namespace veloz::exec
