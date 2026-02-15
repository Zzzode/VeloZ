#pragma once

#include <format>
#include <kj/common.h>
#include <kj/exception.h>
#include <kj/string.h>
#include <source_location>
#include <stdexcept> // std::runtime_error used as base class for exception hierarchy
#include <string>    // std::string used for std::runtime_error compatibility

namespace veloz::core {

class VeloZException : public std::runtime_error {
public:
  explicit VeloZException(std::string_view message,
                          const std::source_location& location = std::source_location::current())
      : std::runtime_error(
            std::format("{} ({}:{}:{})", message,
                        std::string_view(location.file_name())
                            .substr(std::string_view(location.file_name()).find_last_of("/\\") + 1),
                        location.line(), location.column())),
        message_(message), file_(location.file_name()), line_(location.line()),
        column_(location.column()), function_(location.function_name()) {}

  [[nodiscard]] const std::string& message() const noexcept {
    return message_;
  }
  [[nodiscard]] const std::string& file() const noexcept {
    return file_;
  }
  [[nodiscard]] int line() const noexcept {
    return line_;
  }
  [[nodiscard]] int column() const noexcept {
    return column_;
  }
  [[nodiscard]] const std::string& function() const noexcept {
    return function_;
  }

private:
  std::string message_;
  std::string file_;
  int line_;
  int column_;
  std::string function_;
};

class NetworkException : public VeloZException {
public:
  explicit NetworkException(std::string_view message, int error_code = 0,
                            const std::source_location& location = std::source_location::current())
      : VeloZException(message, location), error_code_(error_code) {}

  [[nodiscard]] int error_code() const noexcept {
    return error_code_;
  }

private:
  int error_code_;
};

class ParseException : public VeloZException {
public:
  explicit ParseException(std::string_view message,
                          const std::source_location& location = std::source_location::current())
      : VeloZException(message, location) {}
};

class ValidationException : public VeloZException {
public:
  explicit ValidationException(std::string_view message, const std::source_location& location =
                                                             std::source_location::current())
      : VeloZException(message, location) {}
};

class TimeoutException : public VeloZException {
public:
  explicit TimeoutException(std::string_view message,
                            const std::source_location& location = std::source_location::current())
      : VeloZException(message, location) {}
};

class ResourceException : public VeloZException {
public:
  explicit ResourceException(std::string_view message,
                             const std::source_location& location = std::source_location::current())
      : VeloZException(message, location) {}
};

class CircuitBreakerException : public VeloZException {
public:
  explicit CircuitBreakerException(
      std::string_view message, std::string_view service_name = "",
      const std::source_location& location = std::source_location::current())
      : VeloZException(message, location), service_name_(service_name) {}

  [[nodiscard]] const std::string& service_name() const noexcept {
    return service_name_;
  }

private:
  std::string service_name_;
};

class RateLimitException : public VeloZException {
public:
  explicit RateLimitException(
      std::string_view message, int64_t retry_after_ms = 0,
      const std::source_location& location = std::source_location::current())
      : VeloZException(message, location), retry_after_ms_(retry_after_ms) {}

  [[nodiscard]] int64_t retry_after_ms() const noexcept {
    return retry_after_ms_;
  }

private:
  int64_t retry_after_ms_;
};

class RetryExhaustedException : public VeloZException {
public:
  explicit RetryExhaustedException(
      std::string_view message, int attempts = 0,
      const std::source_location& location = std::source_location::current())
      : VeloZException(message, location), attempts_(attempts) {}

  [[nodiscard]] int attempts() const noexcept {
    return attempts_;
  }

private:
  int attempts_;
};

class ProtocolException : public VeloZException {
public:
  explicit ProtocolException(std::string_view message, int protocol_version = 0,
                             const std::source_location& location = std::source_location::current())
      : VeloZException(message, location), protocol_version_(protocol_version) {}

  [[nodiscard]] int protocol_version() const noexcept {
    return protocol_version_;
  }

private:
  int protocol_version_;
};

// Error code definitions
enum class ErrorCode : int {
  Success = 0,
  UnknownError = 1,
  NetworkError = 2,
  ParseError = 3,
  ValidationError = 4,
  TimeoutError = 5,
  ResourceError = 6,
  ProtocolError = 7,
  NotFoundError = 8,
  PermissionError = 9,
  ConfigurationError = 10,
  StateError = 11,
  CircuitBreakerError = 12,
  RateLimitError = 13,
  RetryExhaustedError = 14,
};

[[nodiscard]] std::string to_string(ErrorCode code);
[[nodiscard]] ErrorCode to_error_code(const std::string& str);

} // namespace veloz::core
