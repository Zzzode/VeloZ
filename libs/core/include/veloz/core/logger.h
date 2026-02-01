#pragma once

#include <cstdint>
#include <mutex>
#include <ostream>
#include <string_view>

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

  void log(LogLevel level, std::string_view message);

  void trace(std::string_view message);
  void debug(std::string_view message);
  void info(std::string_view message);
  void warn(std::string_view message);
  void error(std::string_view message);
  void critical(std::string_view message);

private:
  std::ostream* out_{nullptr};
  LogLevel level_{LogLevel::Info};
  mutable std::mutex mu_;
};

[[nodiscard]] std::string_view to_string(LogLevel level);

} // namespace veloz::core
