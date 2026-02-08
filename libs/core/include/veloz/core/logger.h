#pragma once

#include <cstdint>
#include <mutex>
#include <ostream>
#include <string_view>
#include <source_location>
#include <format>

namespace veloz::core {

enum class LogLevel : std::uint8_t {
  Trace = 0,
  Debug = 1,
  Info = 2,
  Warn = 3,
  Error = 4,
  Critical = 5,
  Off = 6,
};

class Logger final {
public:
  explicit Logger(std::ostream& out);

  void set_level(LogLevel level);
  [[nodiscard]] LogLevel level() const;

  void log(LogLevel level, std::string_view message,
           const std::source_location& location = std::source_location::current());

  void trace(std::string_view message,
             const std::source_location& location = std::source_location::current());
  void debug(std::string_view message,
             const std::source_location& location = std::source_location::current());
  void info(std::string_view message,
            const std::source_location& location = std::source_location::current());
  void warn(std::string_view message,
            const std::source_location& location = std::source_location::current());
  void error(std::string_view message,
             const std::source_location& location = std::source_location::current());
  void critical(std::string_view message,
                const std::source_location& location = std::source_location::current());

  template <typename... Args>
  void log(LogLevel level, std::format_string<Args...> fmt, Args&&... args,
           const std::source_location& location = std::source_location::current()) {
    log(level, std::vformat(fmt.get(), std::make_format_args(args...)), location);
  }

  template <typename... Args>
  void trace(std::format_string<Args...> fmt, Args&&... args,
             const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Trace, fmt, std::forward<Args>(args)..., location);
  }

  template <typename... Args>
  void debug(std::format_string<Args...> fmt, Args&&... args,
             const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Debug, fmt, std::forward<Args>(args)..., location);
  }

  template <typename... Args>
  void info(std::format_string<Args...> fmt, Args&&... args,
            const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Info, fmt, std::forward<Args>(args)..., location);
  }

  template <typename... Args>
  void warn(std::format_string<Args...> fmt, Args&&... args,
            const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Warn, fmt, std::forward<Args>(args)..., location);
  }

  template <typename... Args>
  void error(std::format_string<Args...> fmt, Args&&... args,
             const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Error, fmt, std::forward<Args>(args)..., location);
  }

  template <typename... Args>
  void critical(std::format_string<Args...> fmt, Args&&... args,
                const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Critical, fmt, std::forward<Args>(args)..., location);
  }

private:
  std::ostream* out_{nullptr};
  LogLevel level_{LogLevel::Info};
  mutable std::mutex mu_;
};

[[nodiscard]] std::string_view to_string(LogLevel level);

} // namespace veloz::core
