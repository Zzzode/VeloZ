#include "veloz/core/logger.h"

#include "veloz/core/time.h"

#include <ostream>

namespace veloz::core {

Logger::Logger(std::ostream& out) : out_(&out) {}

void Logger::set_level(LogLevel level) {
  std::scoped_lock lock(mu_);
  level_ = level;
}

LogLevel Logger::level() const {
  std::scoped_lock lock(mu_);
  return level_;
}

void Logger::log(LogLevel level, std::string_view message) {
  std::scoped_lock lock(mu_);
  if (out_ == nullptr) {
    return;
  }
  if (level_ == LogLevel::Off || static_cast<int>(level) < static_cast<int>(level_)) {
    return;
  }
  (*out_) << now_utc_iso8601() << " [" << to_string(level) << "] " << message << '\n';
  out_->flush();
}

void Logger::trace(std::string_view message) {
  log(LogLevel::Trace, message);
}
void Logger::debug(std::string_view message) {
  log(LogLevel::Debug, message);
}
void Logger::info(std::string_view message) {
  log(LogLevel::Info, message);
}
void Logger::warn(std::string_view message) {
  log(LogLevel::Warn, message);
}
void Logger::error(std::string_view message) {
  log(LogLevel::Error, message);
}
void Logger::critical(std::string_view message) {
  log(LogLevel::Critical, message);
}

std::string_view to_string(LogLevel level) {
  switch (level) {
  case LogLevel::Trace:
    return "TRACE";
  case LogLevel::Debug:
    return "DEBUG";
  case LogLevel::Info:
    return "INFO";
  case LogLevel::Warn:
    return "WARN";
  case LogLevel::Error:
    return "ERROR";
  case LogLevel::Critical:
    return "CRITICAL";
  case LogLevel::Off:
    return "OFF";
  }
  return "UNKNOWN";
}

} // namespace veloz::core
