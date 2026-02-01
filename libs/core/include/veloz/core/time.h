#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace veloz::core {

[[nodiscard]] std::int64_t now_unix_ns();
[[nodiscard]] std::string now_utc_iso8601();

} // namespace veloz::core
