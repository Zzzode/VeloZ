#pragma once

#include <kj/compat/http.h>
#include <kj/string.h>
#include <kj/time.h>

namespace veloz::gateway::util {

/**
 * @brief HTTP utility functions for gateway.
 */

/**
 * @brief Extract content type from path.
 *
 * @param path File path or URL path
 * @return Content-Type header value (e.g., "application/json")
 */
kj::String getContentType(kj::StringPtr path);

/**
 * @brief Check if path is a static file request.
 *
 * @param path Request path
 * @return true if path appears to be a static file
 */
bool isStaticFileRequest(kj::StringPtr path);

/**
 * @brief Normalize a path by ensuring it starts with / and has no trailing slash.
 *
 * @param path Path to normalize
 * @return Normalized path
 */
kj::String normalizePath(kj::StringPtr path);

/**
 * @brief Get client IP address from headers.
 *
 * @param headers HTTP headers
 * @return Client IP as string, or "unknown" if not found
 */
kj::String getClientIP(const kj::HttpHeaders& headers);

/**
 * @brief Get rate limit identifier from auth info.
 *
 * If user is authenticated, uses key_id or user_id.
 * Otherwise uses client IP.
 *
 * @param authInfo Authentication info (may be null)
 * @param clientIP Client IP address
 * @return Rate limit identifier string
 */
kj::String getRateLimitIdentifier(const kj::Maybe<auth::AuthInfo>& authInfo,
                                  kj::StringPtr clientIP);

/**
 * @brief Format timestamp as ISO 8601 string.
 *
 * @param timestamp Unix timestamp in nanoseconds
 * @return ISO 8601 formatted string
 */
kj::String formatTimestampISO8601(uint64_t timestampNs);

/**
 * @brief Format timestamp as Unix timestamp string.
 *
 * @param timestampNs Unix timestamp in nanoseconds
 * @param precision Decimal places (0 for seconds, 3 for milliseconds)
 * @return Formatted timestamp string
 */
kj::String formatTimestamp(uint64_t timestampNs, int precision = 0);

/**
 * @brief Parse Last-Event-ID header for SSE.
 *
 * @param headers HTTP headers
 * @return Last event ID, or 0 if not present
 */
uint64_t parseLastEventID(const kj::HttpHeaders& headers);

/**
 * @brief Create CORS headers.
 *
 * @param origin Origin from request
 * @param methods Allowed methods
 * @param headers Allowed headers
 * @param maxAge Max age in seconds
 * @return Vector of (name, value) header pairs
 */
kj::Vector<kj::Tuple<kj::String, kj::String>> createCorsHeaders(kj::StringPtr origin,
                                                                kj::ArrayPtr<kj::String> methods,
                                                                kj::ArrayPtr<kj::String> headers,
                                                                uint32_t maxAge);

/**
 * @brief Check if method is safe (no side effects).
 *
 * @param method HTTP method
 * @return true if method is GET or HEAD
 */
bool isSafeMethod(kj::HttpMethod method);

/**
 * @brief Check if method is cacheable.
 *
 * @param method HTTP method
 * @return true if method is GET
 */
bool isCacheableMethod(kj::HttpMethod method);

/**
 * @brief Get HTTP method as string.
 *
 * @param method HTTP method enum
 * @return Method name (e.g., "GET", "POST")
 */
kj::String getMethodName(kj::HttpMethod method);

/**
 * @brief Parse HTTP method from string.
 *
 * @param methodStr Method name string
 * @return HTTP method enum, or null if unknown
 */
kj::Maybe<kj::HttpMethod> parseMethod(kj::StringPtr methodStr);

} // namespace veloz::gateway::util
