/**
 * @file logger.h
 * @brief Core interfaces and implementation for the logging system
 *
 * This file contains the core interfaces and implementation for the logging system
 * in the VeloZ quantitative trading framework, including log level definitions,
 * logger class, and logging output functionality.
 *
 * The logging system supports multiple log levels including Trace, Debug, Info, Warn,
 * Error, and Critical, and provides formatted output and source location tracking.
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <ostream>
#include <string_view>
#include <source_location>
#include <format>

namespace veloz::core {

/**
 * @brief Log level enumeration
 *
 * Defines the log levels supported by the framework, from Trace (most detailed)
 * to Critical (most severe). The Off level can be used to disable all log output.
 */
enum class LogLevel : std::uint8_t {
  Trace = 0,      ///< Trace level, most detailed debug information
  Debug = 1,      ///< Debug level, for debugging information during development
  Info = 2,       ///< Info level, for normal runtime information
  Warn = 3,       ///< Warning level, for potential issues or warnings
  Error = 4,      ///< Error level, for error information
  Critical = 5,   ///< Critical error level, for errors that prevent system from continuing
  Off = 6,        ///< Turn off all log output
};

/**
 * @brief Logger class
 *
 * Provides thread-safe logging functionality, supporting multiple log levels
 * and formatted output. The logger can output to any standard output stream
 * and provides source location tracking.
 */
class Logger final {
public:
  /**
   * @brief Constructor
   * @param out Output stream, defaults to std::cout
   */
  explicit Logger(std::ostream& out);

  /**
   * @brief Set log level
   * @param level Log level to set
   */
  void set_level(LogLevel level);

  /**
   * @brief Get current log level
   * @return Current log level
   */
  [[nodiscard]] LogLevel level() const;

  /**
   * @brief General log recording method
   * @param level Log level
   * @param message Log message
   * @param location Source location information, defaults to current location
   */
  void log(LogLevel level, std::string_view message,
           const std::source_location& location = std::source_location::current());

  /**
   * @brief Trace level logging
   * @param message Log message
   * @param location Source location information, defaults to current location
   */
  void trace(std::string_view message,
             const std::source_location& location = std::source_location::current());

  /**
   * @brief Debug level logging
   * @param message Log message
   * @param location Source location information, defaults to current location
   */
  void debug(std::string_view message,
             const std::source_location& location = std::source_location::current());

  /**
   * @brief Info level logging
   * @param message Log message
   * @param location Source location information, defaults to current location
   */
  void info(std::string_view message,
            const std::source_location& location = std::source_location::current());

  /**
   * @brief Warning level logging
   * @param message Log message
   * @param location Source location information, defaults to current location
   */
  void warn(std::string_view message,
            const std::source_location& location = std::source_location::current());

  /**
   * @brief Error level logging
   * @param message Log message
   * @param location Source location information, defaults to current location
   */
  void error(std::string_view message,
             const std::source_location& location = std::source_location::current());

  /**
   * @brief Critical error level logging
   * @param message Log message
   * @param location Source location information, defaults to current location
   */
  void critical(std::string_view message,
                const std::source_location& location = std::source_location::current());

  /**
   * @brief Formatted log recording method
   * @tparam Args Formatting parameter types
   * @param level Log level
   * @param fmt Format string
   * @param args Formatting parameters
   * @param location Source location information, defaults to current location
   */
  template <typename... Args>
  void log(LogLevel level, std::format_string<Args...> fmt, Args&&... args,
           const std::source_location& location = std::source_location::current()) {
    log(level, std::vformat(fmt.get(), std::make_format_args(args...)), location);
  }

  /**
   * @brief Formatted trace level logging
   * @tparam Args Formatting parameter types
   * @param fmt Format string
   * @param args Formatting parameters
   * @param location Source location information, defaults to current location
   */
  template <typename... Args>
  void trace(std::format_string<Args...> fmt, Args&&... args,
             const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Trace, fmt, std::forward<Args>(args)..., location);
  }

  /**
   * @brief Formatted debug level logging
   * @tparam Args Formatting parameter types
   * @param fmt Format string
   * @param args Formatting parameters
   * @param location Source location information, defaults to current location
   */
  template <typename... Args>
  void debug(std::format_string<Args...> fmt, Args&&... args,
             const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Debug, fmt, std::forward<Args>(args)..., location);
  }

  /**
   * @brief Formatted info level logging
   * @tparam Args Formatting parameter types
   * @param fmt Format string
   * @param args Formatting parameters
   * @param location Source location information, defaults to current location
   */
  template <typename... Args>
  void info(std::format_string<Args...> fmt, Args&&... args,
            const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Info, fmt, std::forward<Args>(args)..., location);
  }

  /**
   * @brief Formatted warning level logging
   * @tparam Args Formatting parameter types
   * @param fmt Format string
   * @param args Formatting parameters
   * @param location Source location information, defaults to current location
   */
  template <typename... Args>
  void warn(std::format_string<Args...> fmt, Args&&... args,
            const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Warn, fmt, std::forward<Args>(args)..., location);
  }

  /**
   * @brief Formatted error level logging
   * @tparam Args Formatting parameter types
   * @param fmt Format string
   * @param args Formatting parameters
   * @param location Source location information, defaults to current location
   */
  template <typename... Args>
  void error(std::format_string<Args...> fmt, Args&&... args,
             const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Error, fmt, std::forward<Args>(args)..., location);
  }

  /**
   * @brief Formatted critical error level logging
   * @tparam Args Formatting parameter types
   * @param fmt Format string
   * @param args Formatting parameters
   * @param location Source location information, defaults to current location
   */
  template <typename... Args>
  void critical(std::format_string<Args...> fmt, Args&&... args,
                const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Critical, fmt, std::forward<Args>(args)..., location);
  }

private:
  std::ostream* out_{nullptr};       ///< Output stream pointer
  LogLevel level_{LogLevel::Info};   ///< Current log level
  mutable std::mutex mu_;            ///< Mutex for thread safety
};

/**
 * @brief Convert LogLevel to string representation
 * @param level Log level
 * @return String representation of the log level
 */
[[nodiscard]] std::string_view to_string(LogLevel level);

} // namespace veloz::core
