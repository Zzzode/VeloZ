/**
 * @file logger.h
 * @brief 日志系统的核心接口和实现
 *
 * 该文件包含了 VeloZ 量化交易框架中日志系统的核心接口和实现，
 * 包括日志级别定义、日志记录器类以及日志输出功能等。
 *
 * 日志系统支持多种日志级别，包括 Trace、Debug、Info、Warn、Error
 * 和 Critical，并提供了格式化输出、源位置追踪等功能。
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
 * @brief 日志级别枚举
 *
 * 定义了框架支持的日志级别，从 Trace（最详细）到 Critical（最严重）。
 * 可以使用 Off 级别来禁用所有日志输出。
 */
enum class LogLevel : std::uint8_t {
  Trace = 0,      ///< 跟踪级别，最详细的调试信息
  Debug = 1,      ///< 调试级别，用于开发过程中的调试信息
  Info = 2,       ///< 信息级别，用于普通的运行时信息
  Warn = 3,       ///< 警告级别，用于潜在的问题或警告信息
  Error = 4,      ///< 错误级别，用于错误信息
  Critical = 5,   ///< 严重错误级别，用于导致系统无法继续运行的错误
  Off = 6,        ///< 关闭所有日志输出
};

/**
 * @brief 日志记录器类
 *
 * 提供了线程安全的日志记录功能，支持多种日志级别和格式化输出。
 * 日志记录器可以输出到任何标准输出流，并提供了源位置追踪功能。
 */
class Logger final {
public:
  /**
   * @brief 构造函数
   * @param out 输出流，默认为 std::cout
   */
  explicit Logger(std::ostream& out);

  /**
   * @brief 设置日志级别
   * @param level 要设置的日志级别
   */
  void set_level(LogLevel level);

  /**
   * @brief 获取当前日志级别
   * @return 当前日志级别
   */
  [[nodiscard]] LogLevel level() const;

  /**
   * @brief 通用日志记录方法
   * @param level 日志级别
   * @param message 日志消息
   * @param location 源位置信息，默认为当前位置
   */
  void log(LogLevel level, std::string_view message,
           const std::source_location& location = std::source_location::current());

  /**
   * @brief 跟踪级别日志记录
   * @param message 日志消息
   * @param location 源位置信息，默认为当前位置
   */
  void trace(std::string_view message,
             const std::source_location& location = std::source_location::current());

  /**
   * @brief 调试级别日志记录
   * @param message 日志消息
   * @param location 源位置信息，默认为当前位置
   */
  void debug(std::string_view message,
             const std::source_location& location = std::source_location::current());

  /**
   * @brief 信息级别日志记录
   * @param message 日志消息
   * @param location 源位置信息，默认为当前位置
   */
  void info(std::string_view message,
            const std::source_location& location = std::source_location::current());

  /**
   * @brief 警告级别日志记录
   * @param message 日志消息
   * @param location 源位置信息，默认为当前位置
   */
  void warn(std::string_view message,
            const std::source_location& location = std::source_location::current());

  /**
   * @brief 错误级别日志记录
   * @param message 日志消息
   * @param location 源位置信息，默认为当前位置
   */
  void error(std::string_view message,
             const std::source_location& location = std::source_location::current());

  /**
   * @brief 严重错误级别日志记录
   * @param message 日志消息
   * @param location 源位置信息，默认为当前位置
   */
  void critical(std::string_view message,
                const std::source_location& location = std::source_location::current());

  /**
   * @brief 格式化日志记录方法
   * @tparam Args 格式化参数类型
   * @param level 日志级别
   * @param fmt 格式化字符串
   * @param args 格式化参数
   * @param location 源位置信息，默认为当前位置
   */
  template <typename... Args>
  void log(LogLevel level, std::format_string<Args...> fmt, Args&&... args,
           const std::source_location& location = std::source_location::current()) {
    log(level, std::vformat(fmt.get(), std::make_format_args(args...)), location);
  }

  /**
   * @brief 格式化跟踪级别日志记录
   * @tparam Args 格式化参数类型
   * @param fmt 格式化字符串
   * @param args 格式化参数
   * @param location 源位置信息，默认为当前位置
   */
  template <typename... Args>
  void trace(std::format_string<Args...> fmt, Args&&... args,
             const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Trace, fmt, std::forward<Args>(args)..., location);
  }

  /**
   * @brief 格式化调试级别日志记录
   * @tparam Args 格式化参数类型
   * @param fmt 格式化字符串
   * @param args 格式化参数
   * @param location 源位置信息，默认为当前位置
   */
  template <typename... Args>
  void debug(std::format_string<Args...> fmt, Args&&... args,
             const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Debug, fmt, std::forward<Args>(args)..., location);
  }

  /**
   * @brief 格式化信息级别日志记录
   * @tparam Args 格式化参数类型
   * @param fmt 格式化字符串
   * @param args 格式化参数
   * @param location 源位置信息，默认为当前位置
   */
  template <typename... Args>
  void info(std::format_string<Args...> fmt, Args&&... args,
            const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Info, fmt, std::forward<Args>(args)..., location);
  }

  /**
   * @brief 格式化警告级别日志记录
   * @tparam Args 格式化参数类型
   * @param fmt 格式化字符串
   * @param args 格式化参数
   * @param location 源位置信息，默认为当前位置
   */
  template <typename... Args>
  void warn(std::format_string<Args...> fmt, Args&&... args,
            const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Warn, fmt, std::forward<Args>(args)..., location);
  }

  /**
   * @brief 格式化错误级别日志记录
   * @tparam Args 格式化参数类型
   * @param fmt 格式化字符串
   * @param args 格式化参数
   * @param location 源位置信息，默认为当前位置
   */
  template <typename... Args>
  void error(std::format_string<Args...> fmt, Args&&... args,
             const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Error, fmt, std::forward<Args>(args)..., location);
  }

  /**
   * @brief 格式化严重错误级别日志记录
   * @tparam Args 格式化参数类型
   * @param fmt 格式化字符串
   * @param args 格式化参数
   * @param location 源位置信息，默认为当前位置
   */
  template <typename... Args>
  void critical(std::format_string<Args...> fmt, Args&&... args,
                const std::source_location& location = std::source_location::current()) {
    log(LogLevel::Critical, fmt, std::forward<Args>(args)..., location);
  }

private:
  std::ostream* out_{nullptr};       ///< 输出流指针
  LogLevel level_{LogLevel::Info};   ///< 当前日志级别
  mutable std::mutex mu_;            ///< 互斥锁，用于线程安全
};

/**
 * @brief 将 LogLevel 转换为字符串表示
 * @param level 日志级别
 * @return 日志级别的字符串表示
 */
[[nodiscard]] std::string_view to_string(LogLevel level);

} // namespace veloz::core
