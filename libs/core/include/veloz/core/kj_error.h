#pragma once

#include <kj/common.h>
#include <kj/exception.h>
#include <kj/string.h>
#include <kj/string-tree.h>
#include <source_location>

namespace veloz::core {

// Base exception type using KJ
class VeloZException {
public:
  explicit VeloZException(kj::StringPtr message,
                        const std::source_location& location = std::source_location::current())
      : exception_(kj::Exception(
            kj::Exception::Type::FAILED,
            location.file_name(),
            location.line(),
            kj::str(message))),
        message_(kj::str(message)),
        file_(kj::str(location.file_name())),
        line_(location.line()),
        column_(location.column()),
        function_(kj::str(location.function_name())) {}

  [[nodiscard]] const kj::Exception& kj_exception() const noexcept {
    return exception_;
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

  // Convert to KJ Exception for throw/catch
  [[nodiscard]] kj::Exception asKjException() const {
    return kj::mv(exception_);
  }

private:
  kj::Exception exception_;
  kj::String message_;
  kj::String file_;
  int line_;
  int column_;
  kj::String function_;
};

class NetworkException : public VeloZException {
public:
  explicit NetworkException(kj::StringPtr message, int error_code = 0,
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
  explicit ParseException(kj::StringPtr message,
                          const std::source_location& location = std::source_location::current())
      : VeloZException(message, location) {}
};

class ValidationException : public VeloZException {
public:
  explicit ValidationException(kj::StringPtr message,
                              const std::source_location& location = std::source_location::current())
      : VeloZException(message, location) {}
};

class TimeoutException : public VeloZException {
public:
  explicit TimeoutException(kj::StringPtr message,
                           const std::source_location& location = std::source_location::current())
      : VeloZException(message, location) {}
};

class ResourceException : public VeloZException {
public:
  explicit ResourceException(kj::StringPtr message,
                            const std::source_location& location = std::source_location::current())
      : VeloZException(message, location) {}
};

class CircuitBreakerException : public VeloZException {
public:
  explicit CircuitBreakerException(
      kj::StringPtr message, kj::StringPtr service_name = ""_kj,
      const std::source_location& location = std::source_location::current())
      : VeloZException(message, location), service_name_(kj::str(service_name)) {}

  [[nodiscard]] kj::StringPtr service_name() const noexcept {
    return service_name_;
  }

private:
  kj::String service_name_;
};

class RateLimitException : public VeloZException {
public:
  explicit RateLimitException(
      kj::StringPtr message, int64_t retry_after_ms = 0,
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
      kj::StringPtr message, int attempts = 0,
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
  explicit ProtocolException(kj::StringPtr message, int protocol_version = 0,
                           const std::source_location& location = std::source_location::current())
      : VeloZException(message, location), protocol_version_(protocol_version) {}

  [[nodiscard]] int protocol_version() const noexcept {
    return protocol_version_;
  }

private:
  int protocol_version_;
};

// Helper macros for creating KJ-style exceptions
#define VELOZ_THROW_EXCEPTION(message) \
  KJ_THROW(kj::Exception(kj::Exception::Type::FAILED, __FILE__, __LINE__, kj::str(message)))

#define VELOZ_REQUIRE(condition, ...) \
  KJ_REQUIRE(condition, __VA_ARGS__)

#define VELOZ_ASSERT(condition, ...) \
  KJ_ASSERT(condition, __VA_ARGS__)

// Helper to convert std::source_location to readable filename
inline kj::String getFilename(const std::source_location& location) {
  kj::StringPtr file_name = location.file_name();
  KJ_IF_SOME(last_slash, file_name.findLast('/')) {
    return kj::str(file_name.slice(last_slash + 1));
  } else KJ_IF_SOME(last_slash, file_name.findLast('\\')) {
    return kj::str(file_name.slice(last_slash + 1));
  } else {
    return kj::str(file_name);
  }
}

// Helper to format exception with location
inline kj::String formatException(kj::StringPtr message,
                                  const std::source_location& location = std::source_location::current()) {
  return kj::str(message, " (", getFilename(location), ":", location.line(),
                  ":", location.column(), ")");
}

} // namespace veloz::core
