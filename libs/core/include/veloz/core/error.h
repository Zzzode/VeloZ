#pragma once

#include <kj/common.h>
#include <kj/debug.h>
#include <kj/exception.h>
#include <kj/string.h>
#include <source_location>

namespace veloz::core {

/**
 * @brief Base exception class using KJ exception infrastructure
 *
 * This class wraps kj::Exception to provide a consistent exception interface
 * across the VeloZ codebase. It captures source location information and
 * integrates with KJ's exception handling mechanisms.
 *
 * Usage:
 *   throw VeloZException("Something went wrong");
 *   // Or use KJ macros directly:
 *   KJ_FAIL_REQUIRE("Something went wrong", context_value);
 */
class VeloZException {
public:
  explicit VeloZException(kj::StringPtr message,
                          kj::Exception::Type type = kj::Exception::Type::FAILED,
                          const std::source_location& location = std::source_location::current())
      : message_(kj::str(message)), file_(kj::str(location.file_name())), line_(location.line()),
        column_(location.column()), function_(kj::str(location.function_name())), type_(type) {}

  // Allow construction from kj::Exception
  explicit VeloZException(kj::Exception&& e)
      : message_(kj::str(e.getDescription())), file_(kj::str(e.getFile())), line_(e.getLine()),
        column_(0), function_(kj::str("")), type_(e.getType()) {}

  virtual ~VeloZException() = default;

  // Move operations
  VeloZException(VeloZException&&) = default;
  VeloZException& operator=(VeloZException&&) = default;

  // Copy operations - need explicit implementation due to kj::String
  VeloZException(const VeloZException& other)
      : message_(kj::str(other.message_)), file_(kj::str(other.file_)), line_(other.line_),
        column_(other.column_), function_(kj::str(other.function_)), type_(other.type_) {}

  VeloZException& operator=(const VeloZException& other) {
    if (this != &other) {
      message_ = kj::str(other.message_);
      file_ = kj::str(other.file_);
      line_ = other.line_;
      column_ = other.column_;
      function_ = kj::str(other.function_);
      type_ = other.type_;
    }
    return *this;
  }

  [[nodiscard]] kj::StringPtr message() const noexcept {
    return message_;
  }
  [[nodiscard]] kj::StringPtr file() const noexcept {
    return file_;
  }
  [[nodiscard]] int line() const noexcept {
    return line_;
  }
  [[nodiscard]] int column() const noexcept {
    return column_;
  }
  [[nodiscard]] kj::StringPtr function() const noexcept {
    return function_;
  }
  [[nodiscard]] kj::Exception::Type type() const noexcept {
    return type_;
  }

  // For compatibility with code expecting what()
  [[nodiscard]] const char* what() const noexcept {
    return message_.cStr();
  }

  // Convert to kj::Exception for throwing via KJ infrastructure
  [[nodiscard]] kj::Exception toKjException() const {
    return kj::Exception(type_, file_.cStr(), line_, kj::str(message_));
  }

  // Throw this exception using KJ infrastructure
  [[noreturn]] void throwException() const {
    kj::throwFatalException(toKjException());
  }

protected:
  kj::String message_;
  kj::String file_;
  int line_;
  int column_;
  kj::String function_;
  kj::Exception::Type type_;
};

/**
 * @brief Network-related exception (connection failures, DNS errors, etc.)
 */
class NetworkException : public VeloZException {
public:
  explicit NetworkException(kj::StringPtr message, int error_code = 0,
                            const std::source_location& location = std::source_location::current())
      : VeloZException(message, kj::Exception::Type::DISCONNECTED, location),
        error_code_(error_code) {}

  [[nodiscard]] int error_code() const noexcept {
    return error_code_;
  }

private:
  int error_code_;
};

/**
 * @brief Parse error exception (JSON parsing, protocol parsing, etc.)
 */
class ParseException : public VeloZException {
public:
  explicit ParseException(kj::StringPtr message,
                          const std::source_location& location = std::source_location::current())
      : VeloZException(message, kj::Exception::Type::FAILED, location) {}
};

/**
 * @brief Validation error exception (invalid input, constraint violations, etc.)
 */
class ValidationException : public VeloZException {
public:
  explicit ValidationException(kj::StringPtr message,
                               const std::source_location& location = std::source_location::current())
      : VeloZException(message, kj::Exception::Type::FAILED, location) {}
};

/**
 * @brief Timeout exception (operation timed out)
 */
class TimeoutException : public VeloZException {
public:
  explicit TimeoutException(kj::StringPtr message,
                            const std::source_location& location = std::source_location::current())
      : VeloZException(message, kj::Exception::Type::OVERLOADED, location) {}
};

/**
 * @brief Resource exception (out of memory, file not found, etc.)
 */
class ResourceException : public VeloZException {
public:
  explicit ResourceException(kj::StringPtr message,
                             const std::source_location& location = std::source_location::current())
      : VeloZException(message, kj::Exception::Type::OVERLOADED, location) {}
};

/**
 * @brief Circuit breaker exception (service protection triggered)
 */
class CircuitBreakerException : public VeloZException {
public:
  explicit CircuitBreakerException(
      kj::StringPtr message, kj::StringPtr service_name = ""_kj,
      const std::source_location& location = std::source_location::current())
      : VeloZException(message, kj::Exception::Type::OVERLOADED, location),
        service_name_(kj::str(service_name)) {}

  // Copy constructor
  CircuitBreakerException(const CircuitBreakerException& other)
      : VeloZException(other), service_name_(kj::str(other.service_name_)) {}

  [[nodiscard]] kj::StringPtr service_name() const noexcept {
    return service_name_;
  }

private:
  kj::String service_name_;
};

/**
 * @brief Rate limit exception (API rate limiting, throttling, etc.)
 */
class RateLimitException : public VeloZException {
public:
  explicit RateLimitException(kj::StringPtr message, int64_t retry_after_ms = 0,
                              const std::source_location& location = std::source_location::current())
      : VeloZException(message, kj::Exception::Type::OVERLOADED, location),
        retry_after_ms_(retry_after_ms) {}

  [[nodiscard]] int64_t retry_after_ms() const noexcept {
    return retry_after_ms_;
  }

private:
  int64_t retry_after_ms_;
};

/**
 * @brief Retry exhausted exception (all retry attempts failed)
 */
class RetryExhaustedException : public VeloZException {
public:
  explicit RetryExhaustedException(kj::StringPtr message, int attempts = 0,
                                   const std::source_location& location = std::source_location::current())
      : VeloZException(message, kj::Exception::Type::FAILED, location), attempts_(attempts) {}

  [[nodiscard]] int attempts() const noexcept {
    return attempts_;
  }

private:
  int attempts_;
};

/**
 * @brief Protocol exception (protocol version mismatch, invalid protocol, etc.)
 */
class ProtocolException : public VeloZException {
public:
  explicit ProtocolException(kj::StringPtr message, int protocol_version = 0,
                             const std::source_location& location = std::source_location::current())
      : VeloZException(message, kj::Exception::Type::FAILED, location),
        protocol_version_(protocol_version) {}

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

[[nodiscard]] kj::String to_string(ErrorCode code);
[[nodiscard]] ErrorCode to_error_code(kj::StringPtr str);

// Helper macros for throwing VeloZ exceptions using KJ infrastructure
#define VELOZ_REQUIRE(condition, ...) KJ_REQUIRE(condition, ##__VA_ARGS__)
#define VELOZ_FAIL_REQUIRE(...) KJ_FAIL_REQUIRE(__VA_ARGS__)
#define VELOZ_ASSERT(condition, ...) KJ_ASSERT(condition, ##__VA_ARGS__)

} // namespace veloz::core
