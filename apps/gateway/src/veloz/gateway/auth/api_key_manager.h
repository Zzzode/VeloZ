#pragma once

#include <chrono>
#include <cstdint>
#include <kj/array.h>
#include <kj/common.h>
#include <kj/hash.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

/**
 * @brief Represents an API key with its metadata
 *
 * Stores the key identifier, hash, user association, permissions,
 * and usage tracking information.
 */
struct ApiKey {
  kj::String key_id;
  kj::Array<uint8_t> key_hash; // SHA256 hash (32 bytes)
  kj::String user_id;
  kj::String name;
  kj::Vector<kj::String> permissions;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point last_used;
  bool revoked = false;

  ApiKey() = default;
  ApiKey(const ApiKey&) = delete;
  ApiKey& operator=(const ApiKey&) = delete;
  ApiKey(ApiKey&&) = default;
  ApiKey& operator=(ApiKey&&) = default;
};

/**
 * @brief Result of key creation containing both the key_id and raw_key
 *
 * The raw_key should be shown to the user exactly once when created.
 * Only the hash is stored internally.
 */
struct ApiKeyPair {
  kj::String key_id;
  kj::String raw_key;
};

/**
 * @brief Manages API keys with secure hashing and thread-safe operations
 *
 * Features:
 * - SHA256 key hashing (keys stored as hashes only)
 * - Key creation with automatic ID generation
 * - Key validation with O(1) lookup by hash
 * - Key revocation
 * - Permission assignment
 * - Last-used tracking
 * - Thread-safe via kj::MutexGuarded
 *
 * Performance target: <10 microseconds per validation
 */
class ApiKeyManager {
public:
  ApiKeyManager();
  ~ApiKeyManager();

  // Non-copyable, movable
  KJ_DISALLOW_COPY_AND_MOVE(ApiKeyManager);

  /**
   * @brief Create a new API key
   *
   * Generates a unique key_id and random raw_key, hashes the raw_key,
   * and stores the key metadata.
   *
   * @param user_id User ID to associate with the key
   * @param name Human-readable name for the key
   * @param permissions List of permission strings to grant
   * @return ApiKeyPair containing key_id and raw_key (shown once)
   */
  ApiKeyPair create_key(kj::StringPtr user_id, kj::StringPtr name,
                        kj::Vector<kj::String> permissions);

  /**
   * @brief Validate an API key and return its metadata
   *
   * Hashes the provided raw_key and looks it up in O(1) time.
   * Updates last_used timestamp on successful validation.
   *
   * @param raw_key The raw API key to validate
   * @return kj::Maybe<ApiKey> The key metadata if valid and not revoked
   */
  kj::Maybe<kj::Own<ApiKey>> validate(kj::StringPtr raw_key);

  /**
   * @brief Revoke an API key by its ID
   *
   * Marks the key as revoked. Revoked keys cannot be used for authentication.
   *
   * @param key_id The key ID to revoke
   * @return true if the key was found and revoked, false otherwise
   */
  bool revoke(kj::StringPtr key_id);

  /**
   * @brief List all keys for a user
   *
   * Returns a list of keys (without hashes) for administrative purposes.
   *
   * @param user_id User ID to list keys for
   * @return Vector of owned ApiKey copies (without sensitive data)
   */
  kj::Vector<kj::Own<ApiKey>> list_keys(kj::StringPtr user_id);

  /**
   * @brief Check if a key has a specific permission
   *
   * @param key The key to check
   * @param permission The permission string to check for
   * @return true if the key has the permission
   */
  static bool has_permission(const ApiKey& key, kj::StringPtr permission);

  /**
   * @brief Get the number of active (non-revoked) keys
   *
   * @return Number of active keys
   */
  size_t active_key_count() const;

private:
  /**
   * @brief Generate a cryptographically secure random key
   *
   * @param length Number of bytes to generate
   * @return Array of random bytes
   */
  static kj::Array<uint8_t> generate_random_bytes(size_t length);

  /**
   * @brief Compute SHA256 hash of input data
   *
   * @param data Data to hash
   * @return 32-byte SHA256 hash
   */
  static kj::Array<uint8_t> sha256_hash(kj::ArrayPtr<const uint8_t> data);

  /**
   * @brief Generate a unique key ID
   *
   * @return 16-character hexadecimal key ID
   */
  kj::String generate_key_id();

  // Internal state structure for mutex protection
  struct State {
    // Map from key_id to ApiKey
    kj::HashMap<kj::String, kj::Own<ApiKey>> keys_by_id;

    // Map from key_hash (as hex string for HashMap compatibility) to key_id
    kj::HashMap<kj::String, kj::String> key_hash_to_id;

    // Counter for generating unique key IDs
    uint64_t next_key_counter = 1;
  };

  kj::MutexGuarded<State> state_;
};

} // namespace veloz::gateway
