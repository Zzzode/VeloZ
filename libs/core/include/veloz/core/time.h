#pragma once

#include <chrono>
#include <cstdint>
#include <kj/common.h>
#include <kj/string.h>
#include <string> // std::string used for ISO8601 format return type compatibility

namespace veloz::core {

[[nodiscard]] std::int64_t now_unix_ns();
[[nodiscard]] std::string now_utc_iso8601();

} // namespace veloz::core
