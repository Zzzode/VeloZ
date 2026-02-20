#pragma once

#include <cstdint>

namespace veloz::engine {

struct EngineConfig final {
  bool stdio_mode{false};

  // Service mode configuration
  std::uint16_t http_port{8080};
  bool enable_http_server{true};

  // Market data configuration
  bool enable_market_data{true};
  bool use_testnet{false}; ///< Use testnet endpoints for exchanges
};

} // namespace veloz::engine
