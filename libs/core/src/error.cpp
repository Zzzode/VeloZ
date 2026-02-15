#include "veloz/core/error.h"

#include <kj/common.h>
#include <kj/string.h>

namespace veloz::core {

std::string to_string(ErrorCode code) {
  switch (code) {
  case ErrorCode::Success:
    return "Success";
  case ErrorCode::UnknownError:
    return "Unknown Error";
  case ErrorCode::NetworkError:
    return "Network Error";
  case ErrorCode::ParseError:
    return "Parse Error";
  case ErrorCode::ValidationError:
    return "Validation Error";
  case ErrorCode::TimeoutError:
    return "Timeout Error";
  case ErrorCode::ResourceError:
    return "Resource Error";
  case ErrorCode::ProtocolError:
    return "Protocol Error";
  case ErrorCode::NotFoundError:
    return "Not Found Error";
  case ErrorCode::PermissionError:
    return "Permission Error";
  case ErrorCode::ConfigurationError:
    return "Configuration Error";
  case ErrorCode::StateError:
    return "State Error";
  case ErrorCode::CircuitBreakerError:
    return "Circuit Breaker Error";
  case ErrorCode::RateLimitError:
    return "Rate Limit Error";
  case ErrorCode::RetryExhaustedError:
    return "Retry Exhausted Error";
  default:
    return "Invalid Error Code";
  }
}

ErrorCode to_error_code(const std::string& str) {
  if (str == "Success" || str == "success") {
    return ErrorCode::Success;
  } else if (str == "Unknown Error" || str == "unknown") {
    return ErrorCode::UnknownError;
  } else if (str == "Network Error" || str == "network") {
    return ErrorCode::NetworkError;
  } else if (str == "Parse Error" || str == "parse") {
    return ErrorCode::ParseError;
  } else if (str == "Validation Error" || str == "validation") {
    return ErrorCode::ValidationError;
  } else if (str == "Timeout Error" || str == "timeout") {
    return ErrorCode::TimeoutError;
  } else if (str == "Resource Error" || str == "resource") {
    return ErrorCode::ResourceError;
  } else if (str == "Protocol Error" || str == "protocol") {
    return ErrorCode::ProtocolError;
  } else if (str == "Not Found Error" || str == "not_found") {
    return ErrorCode::NotFoundError;
  } else if (str == "Permission Error" || str == "permission") {
    return ErrorCode::PermissionError;
  } else if (str == "Configuration Error" || str == "configuration") {
    return ErrorCode::ConfigurationError;
  } else if (str == "State Error" || str == "state") {
    return ErrorCode::StateError;
  } else if (str == "Circuit Breaker Error" || str == "circuit_breaker") {
    return ErrorCode::CircuitBreakerError;
  } else if (str == "Rate Limit Error" || str == "rate_limit") {
    return ErrorCode::RateLimitError;
  } else if (str == "Retry Exhausted Error" || str == "retry_exhausted") {
    return ErrorCode::RetryExhaustedError;
  } else {
    return ErrorCode::UnknownError;
  }
}

} // namespace veloz::core
