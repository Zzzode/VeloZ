/**
 * @file logger.h
 * @brief Core interfaces and implementation for the logging system
 *
 * This file contains the core interfaces and implementation for the logging system
 * in the VeloZ quantitative trading framework, including log level definitions,
 * logger class, formatters, output destinations, and log rotation support.
 *
 * The logging system supports multiple log levels including Trace, Debug, Info, Warn,
 * Error, and Critical, and provides formatted output and source location tracking.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <kj/common.h>
#include <kj/filesystem.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <source_location>
#include <string> // std::string used for external API compatibility (std::format, std::filesystem)
#include <string_view>
#include <vector> // std::vector used for STL container compatibility

namespace veloz::core {

/**
 * @brief Log level enumeration
 *
 * Defines the log levels supported by the framework, from Trace (most detailed)
 * to Critical (most severe). The Off level can be used to disable all log output.
 */
enum class LogLevel : std::uint8_t {
  Trace = 0,    ///< Trace level, most detailed debug information
  Debug = 1,    ///< Debug level, for debugging information during development
  Info = 2,     ///< Info level, for normal runtime information
  Warn = 3,     ///< Warning level, for potential issues or warnings
  Error = 4,    ///< Error level, for error information
  Critical = 5, ///< Critical error level, for errors that prevent system from continuing
  Off = 6,      ///< Turn off all log output
};

/**
 * @brief Convert LogLevel to string representation
 * @param level Log level
 * @return String representation of the log level
 */
[[nodiscard]] std::string_view to_string(LogLevel level);

/**
 * @brief Log entry containing all log information
 */
struct LogEntry {
  LogLevel level;
  std::string timestamp;
  std::string file;
  int_least32_t line;
  std::string function;
  std::string message;
  std::chrono::system_clock::time_point time_point;
};

/**
 * @brief Base class for log formatters
 *
 * Formatters determine how log entries are represented as strings.
 */
class LogFormatter {
public:
  virtual ~LogFormatter() = default;

  /**
   * @brief Format a log entry to a string
   * @param entry The log entry to format
   * @return Formatted string
   */
  [[nodiscard]] virtual std::string format(const LogEntry& entry) const = 0;

  /**
   * @brief Get the name of this formatter
   */
  [[nodiscard]] virtual std::string_view name() const = 0;
};

/**
 * @brief Text formatter for human-readable log output
 *
 * Format: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] file:line - message
 */
class TextFormatter final : public LogFormatter {
public:
  /**
   * @brief Constructor
   * @param include_function Whether to include function name in output
   * @param use_color Whether to use ANSI color codes (for terminal output)
   */
  explicit TextFormatter(bool include_function = false, bool use_color = false)
      : include_function_(include_function), use_color_(use_color) {}

  [[nodiscard]] std::string format(const LogEntry& entry) const override;
  [[nodiscard]] std::string_view name() const override {
    return "TextFormatter";
  }

private:
  bool include_function_;
  bool use_color_;

  [[nodiscard]] std::string colorize(LogLevel level, std::string_view text) const;
};

/**
 * @brief JSON formatter for structured log output
 *
 * Produces JSON objects with fields: timestamp, level, file, line, function, message
 */
class JsonFormatter final : public LogFormatter {
public:
  /**
   * @brief Constructor
   * @param pretty Whether to pretty-print the JSON with indentation
   */
  explicit JsonFormatter(bool pretty = false) : pretty_(pretty) {}

  [[nodiscard]] std::string format(const LogEntry& entry) const override;
  [[nodiscard]] std::string_view name() const override {
    return "JsonFormatter";
  }

private:
  bool pretty_;
  std::string escape_json(std::string_view s) const;
};

/**
 * @brief Base class for log output destinations
 *
 * Output destinations determine where log entries are written.
 */
class LogOutput {
public:
  virtual ~LogOutput() = default;

  /**
   * @brief Write a formatted log entry
   * @param formatted The formatted log string
   * @param entry The original log entry (for additional processing)
   */
  virtual void write(const std::string& formatted, const LogEntry& entry) = 0;

  /**
   * @brief Flush any buffered output
   */
  virtual void flush() = 0;

  /**
   * @brief Check if this output destination is open/available
   */
  [[nodiscard]] virtual bool is_open() const = 0;
};

/**
 * @brief Console output destination for stdout/stderr
 *
 * Redirects logs to standard output or standard error.
 * Error and Critical messages go to stderr, others to stdout.
 */
class ConsoleOutput final : public LogOutput {
public:
  /**
   * @brief Constructor
   * @param use_stderr If true, all output goes to stderr. If false, error levels go to stderr.
   */
  explicit ConsoleOutput(bool use_stderr = false) : use_stderr_(use_stderr) {}

  void write(const std::string& formatted, const LogEntry& entry) override;
  void flush() override;
  [[nodiscard]] bool is_open() const override {
    return true;
  }

private:
  bool use_stderr_;
};

/**
 * @brief File output destination with log rotation support
 *
 * Writes logs to a file with automatic rotation based on:
 * - Size-based rotation (when file exceeds size limit)
 * - Time-based rotation (at specified intervals)
 */
class FileOutput final : public LogOutput {
public:
  /**
   * @brief Rotation strategy
   */
  enum class Rotation {
    None, ///< No rotation
    Size, ///< Rotate when file exceeds size limit
    Time, ///< Rotate at time intervals
    Both  ///< Use both size and time-based rotation
  };

  /**
   * @brief Time interval for time-based rotation
   */
  enum class RotationInterval { Hourly, Daily, Weekly, Monthly };

  /**
   * @brief Constructor
   * @param file_path Base path for log files
   * @param rotation Rotation strategy
   * @param max_size Maximum file size in bytes before rotation (for Size rotation)
   * @param max_files Maximum number of backup files to keep
   * @param interval Time interval for rotation (for Time rotation)
   */
  explicit FileOutput(const kj::Path& file_path, Rotation rotation = Rotation::Size,
                      size_t max_size = 10 * 1024 * 1024, // 10MB default
                      size_t max_files = 5, RotationInterval interval = RotationInterval::Daily);

  ~FileOutput() noexcept override;

  void write(const std::string& formatted, const LogEntry& entry) override;
  void flush() override;
  [[nodiscard]] bool is_open() const override;

  /**
   * @brief Force rotation now
   */
  void rotate();

  /**
   * @brief Get current file path
   */
  [[nodiscard]] kj::Path current_path() const;

  /**
   * @brief Get rotation settings
   */
  [[nodiscard]] Rotation rotation() const {
    return guarded_.getWithoutLock().rotation;
  }
  [[nodiscard]] size_t max_size() const {
    return guarded_.getWithoutLock().max_size;
  }
  [[nodiscard]] size_t max_files() const {
    return max_files_;
  }

private:
  // Internal state for FileOutput
  struct FileOutputState {
    kj::Path file_path;
    std::ofstream file_stream;
    size_t current_size;
    std::chrono::system_clock::time_point last_rotation;
    const size_t max_size;
    const Rotation rotation;
    const RotationInterval interval;

    FileOutputState(const kj::Path& path, size_t max_sz, Rotation rot,
                    RotationInterval interv)
        : file_path(path.clone()), current_size(0), last_rotation(std::chrono::system_clock::now()),
          max_size(max_sz), rotation(rot), interval(interv) {}
  };

  void writeImpl(FileOutputState& state, const std::string& formatted);
  void check_rotation();
  [[nodiscard]] kj::Path get_rotated_path(size_t index) const;
  void perform_rotation(FileOutputState& state);
  [[nodiscard]] bool should_rotate_by_time(const FileOutputState& state) const;
  [[nodiscard]] std::string get_rotation_suffix() const;

  kj::MutexGuarded<FileOutputState> guarded_;
  const size_t max_files_;
};

/**
 * @brief Multi-output destination
 *
 * Writes to multiple output destinations simultaneously.
 */
class MultiOutput final : public LogOutput {
public:
  explicit MultiOutput() = default;

  void add_output(kj::Own<LogOutput> output);
  void remove_output(size_t index);
  void clear_outputs();

  void write(const std::string& formatted, const LogEntry& entry) override;
  void flush() override;
  [[nodiscard]] bool is_open() const override;

  [[nodiscard]] size_t output_count() const;

private:
  // Internal state for MultiOutput
  struct MultiOutputState {
    std::vector<kj::Own<LogOutput>> outputs;
  };

  kj::MutexGuarded<MultiOutputState> guarded_;
};

/**
 * @brief Logger class
 *
 * Provides thread-safe logging functionality, supporting multiple log levels,
 * formatters, and output destinations with log rotation support.
 */
class Logger final {
public:
  /**
   * @brief Constructor
   * @param formatter Log formatter to use
   * @param output Log output destination
   */
  Logger(kj::Own<LogFormatter> formatter = kj::heap<TextFormatter>(),
         kj::Own<LogOutput> output = kj::heap<ConsoleOutput>());

  ~Logger();

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
   * @brief Set log formatter
   * @param formatter Formatter to use
   */
  void set_formatter(kj::Own<LogFormatter> formatter);

  /**
   * @brief Set log output destination
   * @param output Output destination
   */
  void set_output(kj::Own<LogOutput> output);

  /**
   * @brief Add additional output destination
   * @param output Output destination to add
   */
  void add_output(kj::Own<LogOutput> output);

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
    log(LogLevel::Trace, std::vformat(fmt.get(), std::make_format_args(args...)), location);
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
    log(LogLevel::Debug, std::vformat(fmt.get(), std::make_format_args(args...)), location);
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
    log(LogLevel::Info, std::vformat(fmt.get(), std::make_format_args(args...)), location);
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
    log(LogLevel::Warn, std::vformat(fmt.get(), std::make_format_args(args...)), location);
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
    log(LogLevel::Error, std::vformat(fmt.get(), std::make_format_args(args...)), location);
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
    log(LogLevel::Critical, std::vformat(fmt.get(), std::make_format_args(args...)), location);
  }

  /**
   * @brief Flush all output buffers
   */
  void flush();

private:
  // Internal state for Logger
  struct LoggerState {
    kj::Own<LogFormatter> formatter;
    kj::Own<MultiOutput> multi_output;
    LogLevel level;

    LoggerState(kj::Own<LogFormatter> fmt, kj::Own<LogOutput> out)
        : formatter(kj::mv(fmt)), multi_output(kj::heap<MultiOutput>()),
          level(LogLevel::Info) {
      multi_output->add_output(kj::mv(out));
    }
  };

  kj::MutexGuarded<LoggerState> guarded_;
};

/**
 * @brief Global logger accessor
 */
[[nodiscard]] Logger& global_logger();

/**
 * @brief Convenience functions for logging to the global logger
 */
inline void log_global(LogLevel level, std::string_view message,
                       const std::source_location& location = std::source_location::current()) {
  global_logger().log(level, message, location);
}

template <typename... Args>
inline void log_global(LogLevel level, std::format_string<Args...> fmt, Args&&... args,
                       const std::source_location& location = std::source_location::current()) {
  global_logger().log(level, fmt, std::forward<Args>(args)..., location);
}

inline void trace_global(std::string_view message,
                         const std::source_location& location = std::source_location::current()) {
  global_logger().trace(message, location);
}

inline void debug_global(std::string_view message,
                         const std::source_location& location = std::source_location::current()) {
  global_logger().debug(message, location);
}

inline void info_global(std::string_view message,
                        const std::source_location& location = std::source_location::current()) {
  global_logger().info(message, location);
}

inline void warn_global(std::string_view message,
                        const std::source_location& location = std::source_location::current()) {
  global_logger().warn(message, location);
}

inline void error_global(std::string_view message,
                         const std::source_location& location = std::source_location::current()) {
  global_logger().error(message, location);
}

inline void
critical_global(std::string_view message,
                const std::source_location& location = std::source_location::current()) {
  global_logger().critical(message, location);
}

template <typename... Args>
inline void trace_global(std::format_string<Args...> fmt, Args&&... args,
                         const std::source_location& location = std::source_location::current()) {
  global_logger().trace(fmt, std::forward<Args>(args)..., location);
}

template <typename... Args>
inline void debug_global(std::format_string<Args...> fmt, Args&&... args,
                         const std::source_location& location = std::source_location::current()) {
  global_logger().debug(fmt, std::forward<Args>(args)..., location);
}

template <typename... Args>
inline void info_global(std::format_string<Args...> fmt, Args&&... args,
                        const std::source_location& location = std::source_location::current()) {
  global_logger().info(fmt, std::forward<Args>(args)..., location);
}

template <typename... Args>
inline void warn_global(std::format_string<Args...> fmt, Args&&... args,
                        const std::source_location& location = std::source_location::current()) {
  global_logger().warn(fmt, std::forward<Args>(args)..., location);
}

template <typename... Args>
inline void error_global(std::format_string<Args...> fmt, Args&&... args,
                         const std::source_location& location = std::source_location::current()) {
  global_logger().error(fmt, std::forward<Args>(args)..., location);
}

template <typename... Args>
inline void
critical_global(std::format_string<Args...> fmt, Args&&... args,
                const std::source_location& location = std::source_location::current()) {
  global_logger().critical(fmt, std::forward<Args>(args)..., location);
}

} // namespace veloz::core
