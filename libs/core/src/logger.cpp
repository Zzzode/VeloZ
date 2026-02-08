#include "veloz/core/logger.h"

#include "veloz/core/time.h"

#include <ostream>
#include <format>

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

void Logger::log(LogLevel level, std::string_view message,
                 const std::source_location& location) {
  std::scoped_lock lock(mu_);
  if (out_ == nullptr) {
    return;
  }
  if (level_ == LogLevel::Off || static_cast<int>(level) < static_cast<int>(level_)) {
    return;
  }

  auto time_str = now_utc_iso8601();
  auto level_str = to_string(level);
  auto file_name = std::string_view(location.file_name()).substr(
      std::string_view(location.file_name()).find_last_of("/\\") + 1);

  (*out_) << std::format("{} [{}] {}:{} - {}\n",
                         time_str, level_str, file_name, location.line(), message);
  out_->flush();
}

void Logger::trace(std::string_view message,
                   const std::source_location& location) {
  log(LogLevel::Trace, message, location);
}

void Logger::debug(std::string_view message,
                   const std::source_location& location) {
  log(LogLevel::Debug, message, location);
}

void Logger::info(std::string_view message,
                  const std::source_location& location) {
  log(LogLevel::Info, message, location);
}

void Logger::warn(std::string_view message,
                  const std::source_location& location) {
  log(LogLevel::Warn, message, location);
}

void Logger::error(std::string_view message,
                   const std::source_location& location) {
  log(LogLevel::Error, message, location);
}

void Logger::critical(std::string_view message,
                      const std::source_location& location) {
  log(LogLevel::Critical, message, location);
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
