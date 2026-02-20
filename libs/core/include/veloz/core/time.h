#pragma once

#include <chrono>
#include <cstdint>
#include <kj/common.h>
#include <kj/string.h>

namespace veloz::core {

[[nodiscard]] std::int64_t now_unix_ns();
[[nodiscard]] kj::String now_utc_iso8601();

} // namespace veloz::core
