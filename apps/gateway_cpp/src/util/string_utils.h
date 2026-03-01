#pragma once

#include <cstdint>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway::util {

/**
 * @brief String utility functions for gateway.
 */

/**
 * @brief Split string by delimiter.
 *
 * @param input String to split
 * @param delimiter Delimiter string
 * @return Vector of parts
 */
kj::Vector<kj::String> split(kj::StringPtr input, kj::StringPtr delimiter);

/**
 * @brief Join strings with delimiter.
 *
 * @param parts Strings to join
 * @param delimiter Delimiter string
 * @return Joined string
 */
kj::String join(kj::ArrayPtr<kj::String> parts, kj::StringPtr delimiter);

/**
 * @brief Trim whitespace from start and end of string.
 *
 * @param input String to trim
 * @return Trimmed string
 */
kj::String trim(kj::StringPtr input);

/**
 * @brief Convert string to lowercase (ASCII only).
 *
 * @param input String to convert
 * @return Lowercase string
 */
kj::String toLower(kj::StringPtr input);

/**
 * @brief Convert string to uppercase (ASCII only).
 *
 * @param input String to convert
 * @return Uppercase string
 */
kj::String toUpper(kj::StringPtr input);

/**
 * @brief Check if string starts with prefix.
 *
 * @param input String to check
 * @param prefix Prefix to look for
 * @return true if string starts with prefix
 */
bool startsWith(kj::StringPtr input, kj::StringPtr prefix);

/**
 * @brief Check if string ends with suffix.
 *
 * @param input String to check
 * @param suffix Suffix to look for
 * @return true if string ends with suffix
 */
bool endsWith(kj::StringPtr input, kj::StringPtr suffix);

/**
 * @brief Check if string equals another (case-insensitive for ASCII).
 *
 * @param a First string
 * @param b Second string
 * @return true if strings are equal (case-insensitive)
 */
bool equalsIgnoreCase(kj::StringPtr a, kj::StringPtr b);

/**
 * @brief Check if string contains substring.
 *
 * @param input String to search in
 * @param search Substring to find
 * @return true if substring is found
 */
bool contains(kj::StringPtr input, kj::StringPtr search);

/**
 * @brief Generate UUID v4 string.
 *
 * @return UUID string (36 characters with hyphens)
 */
kj::String generateUUID();

/**
 * @brief Generate random hex string.
 *
 * @param length Number of bytes (output will be 2x characters)
 * @return Hex-encoded random string
 */
kj::String generateRandomHex(size_t length);

/**
 * @brief Hash string using SHA-256.
 *
 * @param input String to hash
 * @return SHA-256 hash as hex string (64 characters)
 */
kj::String sha256(kj::StringPtr input);

/**
 * @brief Base64 encode bytes.
 *
 * @param data Bytes to encode
 * @return Base64 encoded string
 */
kj::String base64Encode(kj::ArrayPtr<const kj::byte> data);

/**
 * @brief Base64 decode string.
 *
 * @param encoded Base64 encoded string
 * @return Decoded bytes, or empty array on error
 */
kj::Array<kj::byte> base64Decode(kj::StringPtr encoded);

/**
 * @brief Base64URL encode bytes (URL-safe variant).
 *
 * @param data Bytes to encode
 * @return Base64URL encoded string
 */
kj::String base64UrlEncode(kj::ArrayPtr<const kj::byte> data);

/**
 * @brief Base64URL decode string.
 *
 * @param encoded Base64URL encoded string
 * @return Decoded bytes, or empty array on error
 */
kj::Array<kj::byte> base64UrlDecode(kj::StringPtr encoded);

/**
 * @brief Check if string looks like an ID (UUID or numeric).
 *
 * @param str String to check
 * @return true if string looks like an ID
 */
bool looksLikeID(kj::StringPtr str);

/**
 * @brief Normalize endpoint path for metrics.
 *
 * Removes IDs, query parameters, and standardizes path.
 * Used to group similar endpoints for metrics.
 *
 * @param path Request path
 * @return Normalized path for metrics
 */
kj::String normalizeEndpointForMetrics(kj::StringPtr path);

/**
 * @brief Format size in human-readable form.
 *
 * @param bytes Number of bytes
 * @return Formatted string (e.g., "1.23 KB")
 */
kj::String formatBytes(size_t bytes);

/**
 * @brief Format duration in human-readable form.
 *
 * @param nanoseconds Duration in nanoseconds
 * @return Formatted string (e.g., "1.23 ms", "1.23 s")
 */
kj::String formatDuration(uint64_t nanoseconds);

} // namespace veloz::gateway::util
