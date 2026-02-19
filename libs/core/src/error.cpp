#include "veloz/core/error.h"

#include <kj/common.h>
#include <kj/string.h>

namespace veloz::core {

kj::String to_string(ErrorCode code) {
  switch (code) {
  case ErrorCode::Success:
    return kj::str("Success");
  case ErrorCode::UnknownError:
    return kj::str("Unknown Error");
  case ErrorCode::NetworkError:
    return kj::str("Network Error");
  case ErrorCode::ParseError:
    return kj::str("Parse Error");
  case ErrorCode::ValidationError:
    return kj::str("Validation Error");
  case ErrorCode::TimeoutError:
    return kj::str("Timeout Error");
  case ErrorCode::ResourceError:
    return kj::str("Resource Error");
  case ErrorCode::ProtocolError:
    return kj::str("Protocol Error");
  case ErrorCode::NotFoundError:
    return kj::str("Not Found Error");
  case ErrorCode::PermissionError:
    return kj::str("Permission Error");
  case ErrorCode::ConfigurationError:
    return kj::str("Configuration Error");
  case ErrorCode::StateError:
    return kj::str("State Error");
  case ErrorCode::CircuitBreakerError:
    return kj::str("Circuit Breaker Error");
  case ErrorCode::RateLimitError:
    return kj::str("Rate Limit Error");
  case ErrorCode::RetryExhaustedError:
    return kj::str("Retry Exhausted Error");
  default:
    return kj::str("Invalid Error Code");
  }
}

ErrorCode to_error_code(kj::StringPtr str) {
  if (str == "Success"_kj || str == "success"_kj) {
    return ErrorCode::Success;
  } else if (str == "Unknown Error"_kj || str == "unknown"_kj) {
    return ErrorCode::UnknownError;
  } else if (str == "Network Error"_kj || str == "network"_kj) {
    return ErrorCode::NetworkError;
  } else if (str == "Parse Error"_kj || str == "parse"_kj) {
    return ErrorCode::ParseError;
  } else if (str == "Validation Error"_kj || str == "validation"_kj) {
    return ErrorCode::ValidationError;
  } else if (str == "Timeout Error"_kj || str == "timeout"_kj) {
    return ErrorCode::TimeoutError;
  } else if (str == "Resource Error"_kj || str == "resource"_kj) {
    return ErrorCode::ResourceError;
  } else if (str == "Protocol Error"_kj || str == "protocol"_kj) {
    return ErrorCode::ProtocolError;
  } else if (str == "Not Found Error"_kj || str == "not_found"_kj) {
    return ErrorCode::NotFoundError;
  } else if (str == "Permission Error"_kj || str == "permission"_kj) {
    return ErrorCode::PermissionError;
  } else if (str == "Configuration Error"_kj || str == "configuration"_kj) {
    return ErrorCode::ConfigurationError;
  } else if (str == "State Error"_kj || str == "state"_kj) {
    return ErrorCode::StateError;
  } else if (str == "Circuit Breaker Error"_kj || str == "circuit_breaker"_kj) {
    return ErrorCode::CircuitBreakerError;
  } else if (str == "Rate Limit Error"_kj || str == "rate_limit"_kj) {
    return ErrorCode::RateLimitError;
  } else if (str == "Retry Exhausted Error"_kj || str == "retry_exhausted"_kj) {
    return ErrorCode::RetryExhaustedError;
  } else {
    return ErrorCode::UnknownError;
  }
}

} // namespace veloz::core
