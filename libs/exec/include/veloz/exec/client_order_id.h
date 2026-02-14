#pragma once

#include <cstdint>
#include <kj/common.h>
#include <kj/string.h>
#include <tuple> // std::tuple has no KJ equivalent

namespace veloz::exec {

class ClientOrderIdGenerator final {
public:
  explicit ClientOrderIdGenerator(kj::StringPtr strategy_id);

  // Generate unique client order ID
  [[nodiscard]] kj::String generate();

  // Parse components: {strategy, timestamp, unique}
  static std::tuple<kj::String, std::int64_t, kj::String> parse(kj::StringPtr client_order_id);

private:
  kj::String strategy_id_;
  std::int64_t sequence_{0};
};

} // namespace veloz::exec
