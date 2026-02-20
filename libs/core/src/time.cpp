#include "veloz/core/time.h"

#include <cstdio>
#include <ctime>
#include <kj/common.h>
#include <kj/string.h>

namespace veloz::core {

std::int64_t now_unix_ns() {
  const auto now = std::chrono::system_clock::now();
  const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
  return static_cast<std::int64_t>(ns.count());
}

kj::String now_utc_iso8601() {
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

  char buf[64];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%09ldZ", tm.tm_year + 1900,
                tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, subsec_ns);
  return kj::str(buf);
}

} // namespace veloz::core
