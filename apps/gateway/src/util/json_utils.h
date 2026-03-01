#pragma once

#include <kj/array.h>
#include <kj/async-io.h>
#include <kj/map.h>
#include <kj/string.h>

namespace veloz::gateway::util {

/**
 * @brief JSON utilities for request/response handling.
 *
 * Provides functions for creating and parsing JSON objects.
 * Uses yyjson for high-performance JSON operations.
 */

/**
 * @brief Simple JSON object builder.
 *
 * Provides fluent API for building JSON objects.
 * All strings are kj::String for KJ compatibility.
 */
class JsonBuilder {
public:
  explicit JsonBuilder();
  ~JsonBuilder();

  // Non-copyable, movable
  JsonBuilder(const JsonBuilder&) = delete;
  JsonBuilder& operator=(const JsonBuilder&) = delete;
  JsonBuilder(JsonBuilder&&) noexcept = default;
  JsonBuilder& operator=(JsonBuilder&&) noexcept = default;

  /**
   * @brief Add a string field to the JSON object.
   */
  void put(kj::StringPtr key, kj::StringPtr value);

  /**
   * @brief Add a numeric field to the JSON object.
   */
  void put(kj::StringPtr key, double value);

  /**
   * @brief Add a boolean field to the JSON object.
   */
  void put(kj::StringPtr key, bool value);

  /**
   * @brief Add a null field to the JSON object.
   */
  void putNull(kj::StringPtr key);

  /**
   * @brief Add a nested object field.
   */
  void putObject(kj::StringPtr key, const JsonBuilder& value);

  /**
   * @brief Add an array field.
   */
  void putArray(kj::StringPtr key, kj::ArrayPtr<kj::String> value);

  /**
   * @brief Build the JSON string.
   *
   * @return JSON string representation
   */
  kj::String build();

private:
  class Impl;
  kj::Own<Impl> impl_;
};

/**
 * @brief Parse JSON string.
 *
 * @param json JSON string to parse
 * @return Parsed value as kj::StringPtr (using yyjson's string representation)
 *
 * @note This is a simplified version. Full JSON parsing
 *       should use yyjson's API directly for better performance.
 */
kj::Maybe<kj::String> parseJsonString(kj::StringPtr json);

/**
 * @brief Check if string is valid JSON.
 *
 * @param json String to check
 * @return true if valid JSON
 */
bool isValidJson(kj::StringPtr json);

/**
 * @brief Create a JSON error response.
 *
 * @param error Error code
 * @param message Human-readable message
 * @return JSON string
 */
kj::String createErrorResponse(kj::StringPtr error, kj::StringPtr message);

/**
 * @brief Create a JSON success response.
 *
 * @param data Additional data to include (optional)
 * @return JSON string
 */
kj::String createSuccessResponse(kj::Maybe<kj::StringPtr> data = kj::none);

/**
 * @brief URL-encode a string for query parameters.
 *
 * @param input String to encode
 * @return URL-encoded string
 */
kj::String urlEncode(kj::StringPtr input);

/**
 * @brief URL-decode a string.
 *
 * @param input String to decode
 * @return URL-decoded string
 */
kj::String urlDecode(kj::StringPtr input);

/**
 * @brief Parse query string from URL.
 *
 * @param queryString Query string (without leading ?)
 * @return Map of parameter name → value
 */
kj::HashMap<kj::String, kj::String> parseQueryString(kj::StringPtr queryString);

} // namespace veloz::gateway::util
