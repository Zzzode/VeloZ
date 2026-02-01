#include "veloz/core/time.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace veloz::core {

std::int64_t now_unix_ns() {
  const auto now = std::chrono::system_clock::now();
  const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
  return static_cast<std::int64_t>(ns.count());
}

std::string now_utc_iso8601() {
  const auto now = std::chrono::system_clock::now();
  const auto now_time_t = std::chrono::system_clock::to_time_t(now);

  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &now_time_t);
#else
  gmtime_r(&now_time_t, &tm);
#endif

  const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
  const auto subsec_ns = static_cast<long>(ns.count() % 1'000'000'000);

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
  oss << '.' << std::setw(9) << std::setfill('0') << subsec_ns << 'Z';
  return oss.str();
}

} // namespace veloz::core
