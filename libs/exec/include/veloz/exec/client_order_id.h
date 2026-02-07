#pragma once

#include <cstdint>
#include <string>
#include <tuple>

namespace veloz::exec {

class ClientOrderIdGenerator final {
public:
    explicit ClientOrderIdGenerator(const std::string& strategy_id);

    // Generate unique client order ID
    [[nodiscard]] std::string generate();

    // Parse components: {strategy, timestamp, unique}
    static std::tuple<std::string, std::int64_t, std::string> parse(const std::string& client_order_id);

private:
    std::string strategy_id_;
    std::int64_t sequence_{0};
};

} // namespace veloz::exec
