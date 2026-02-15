#pragma once

#include <cstdint>
#include <kj/common.h>
#include <kj/string.h>

namespace veloz::exec {

/**
 * @brief Parsed components of a client order ID
 *
 * This struct replaces std::tuple for the parse() return type, providing
 * named fields for better readability and KJ-style API design.
 */
struct ParsedClientOrderId {
  kj::String strategy;     ///< Strategy identifier
  std::int64_t timestamp;  ///< Unix timestamp in seconds
  kj::String unique;       ///< Unique component (sequence-random or empty)
};

class ClientOrderIdGenerator final {
public:
  explicit ClientOrderIdGenerator(kj::StringPtr strategy_id);

  // Generate unique client order ID
  [[nodiscard]] kj::String generate();

  // Parse components from client order ID string
  [[nodiscard]] static ParsedClientOrderId parse(kj::StringPtr client_order_id);

private:
  kj::String strategy_id_;
  std::int64_t sequence_{0};
};

} // namespace veloz::exec
