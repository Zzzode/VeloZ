#include "veloz/exec/client_order_id.h"
#include <chrono>
#include <random>
#include <sstream>

namespace veloz::exec {

ClientOrderIdGenerator::ClientOrderIdGenerator(const std::string& strategy_id)
    : strategy_id_(strategy_id) {}

std::string ClientOrderIdGenerator::generate() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    ++sequence_;

    // Generate random component for uniqueness across processes
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_int_distribution<> dis(0, 15);

    std::ostringstream oss;
    oss << strategy_id_ << "-" << timestamp << "-" << sequence_ << "-";
    for (int i = 0; i < 4; ++i) {
        oss << std::hex << dis(gen);
    }

    return oss.str();
}

std::tuple<std::string, std::int64_t, std::string>
ClientOrderIdGenerator::parse(const std::string& client_order_id) {
    // Format: STRATEGY-TIMESTAMP-SEQUENCE-RANDOM
    size_t first_dash = client_order_id.find('-');
    size_t second_dash = client_order_id.find('-', first_dash + 1);
    size_t third_dash = client_order_id.find('-', second_dash + 1);

    if (first_dash == std::string::npos ||
        second_dash == std::string::npos ||
        third_dash == std::string::npos) {
        return {"", 0, ""};
    }

    std::string strategy = client_order_id.substr(0, first_dash);
    std::int64_t timestamp = std::stoll(client_order_id.substr(first_dash + 1, second_dash - first_dash - 1));
    std::string unique = client_order_id.substr(third_dash + 1);

    return {strategy, timestamp, unique};
}

} // namespace veloz::exec
