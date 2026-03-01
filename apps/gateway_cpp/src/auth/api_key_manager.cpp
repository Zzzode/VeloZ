#include "api_key_manager.h"

#include <kj/debug.h>
#include <kj/string.h>

// OpenSSL for SHA256
#include <openssl/evp.h>
#include <openssl/rand.h>

// std::random for entropy (KJ lacks RNG)
#include <iomanip>
#include <random>
#include <sstream>

namespace veloz::gateway {

namespace {

constexpr size_t RAW_KEY_BYTES = 32; // 256-bit key
constexpr size_t SHA256_HASH_BYTES = 32;
constexpr size_t KEY_ID_LENGTH = 16;

} // anonymous namespace

ApiKeyManager::ApiKeyManager() = default;

ApiKeyManager::~ApiKeyManager() = default;

ApiKeyPair ApiKeyManager::create_key(kj::StringPtr user_id, kj::StringPtr name,
                                     kj::Vector<kj::String> permissions) {
  // Generate random raw key
  kj::Array<uint8_t> raw_bytes = generate_random_bytes(RAW_KEY_BYTES);

  // Generate hex-encoded raw key for user
  static constexpr char HEX_MAP[] = "0123456789abcdef";
  kj::String raw_key = kj::heapString(RAW_KEY_BYTES * 2);
  for (size_t i = 0; i < RAW_KEY_BYTES; ++i) {
    raw_key[i * 2] = HEX_MAP[(raw_bytes[i] >> 4) & 0x0F];
    raw_key[i * 2 + 1] = HEX_MAP[raw_bytes[i] & 0x0F];
  }

  // Generate key ID
  kj::String key_id = generate_key_id();

  // Compute SHA256 hash
  kj::Array<uint8_t> key_hash = sha256_hash(raw_bytes);

  // Get current timestamp
  auto now = std::chrono::system_clock::now();

  // Create ApiKey object
  auto api_key = kj::heap<ApiKey>();
  api_key->key_id = kj::mv(key_id);
  api_key->key_hash = kj::mv(key_hash);
  api_key->user_id = kj::str(user_id);
  api_key->name = kj::str(name);
  api_key->permissions = kj::mv(permissions);
  api_key->created_at = now;
  api_key->last_used = now;
  api_key->revoked = false;

  // Convert hash to hex string for hash map key
  kj::String hash_hex = kj::heapString(SHA256_HASH_BYTES * 2);
  for (size_t i = 0; i < SHA256_HASH_BYTES; ++i) {
    hash_hex[i * 2] = HEX_MAP[(api_key->key_hash[i] >> 4) & 0x0F];
    hash_hex[i * 2 + 1] = HEX_MAP[api_key->key_hash[i] & 0x0F];
  }

  // Store in maps (thread-safe)
  // Save key_id before moving api_key
  kj::String key_id_copy = kj::str(api_key->key_id);
  kj::String hash_hex_copy = kj::mv(hash_hex);

  {
    auto lock = state_.lockExclusive();

    // Check for hash collision (should be astronomically unlikely)
    KJ_IF_SOME(_, lock->key_hash_to_id.find(hash_hex_copy)) {
      // Hash collision detected - regenerate key ID and retry
      // In practice, this will never happen with SHA256
      KJ_FAIL_REQUIRE("SHA256 hash collision detected - cryptographic failure",
                      hash_hex_copy.cStr());
    }

    // Store in key_id -> ApiKey map
    lock->keys_by_id.upsert(kj::str(key_id_copy), kj::mv(api_key),
                            [](kj::Own<ApiKey>&, kj::Own<ApiKey>&&) {});

    // Store in hash -> key_id map
    kj::String hash_key = kj::str(hash_hex_copy);
    lock->key_hash_to_id.upsert(kj::mv(hash_key), kj::str(key_id_copy),
                                [](kj::String&, kj::String&&) {});
  }

  // Return the key pair with raw key
  ApiKeyPair result;
  result.key_id = kj::mv(key_id_copy);
  result.raw_key = kj::mv(raw_key);
  return result;
}

kj::Maybe<kj::Own<ApiKey>> ApiKeyManager::validate(kj::StringPtr raw_key) {
  // Parse hex raw key back to bytes
  // Return none for invalid format instead of throwing
  if (raw_key.size() != RAW_KEY_BYTES * 2) {
    return kj::none;
  }

  kj::Array<uint8_t> raw_bytes = kj::heapArray<uint8_t>(RAW_KEY_BYTES);
  static constexpr int from_hex[] = {
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  -1, -1, -1, -1, -1, -1, -1, 10,
      11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  };

  for (size_t i = 0; i < RAW_KEY_BYTES; ++i) {
    int hi = from_hex[static_cast<uint8_t>(raw_key[i * 2])];
    int lo = from_hex[static_cast<uint8_t>(raw_key[i * 2 + 1])];
    if (hi < 0 || lo < 0) {
      return kj::none; // Invalid hex character
    }
    raw_bytes[i] = static_cast<uint8_t>((hi << 4) | lo);
  }

  // Compute hash of the raw key
  kj::Array<uint8_t> key_hash = sha256_hash(raw_bytes);

  // Convert to hex string for lookup
  static constexpr char HEX_MAP[] = "0123456789abcdef";
  kj::String hash_hex = kj::heapString(SHA256_HASH_BYTES * 2);
  for (size_t i = 0; i < SHA256_HASH_BYTES; ++i) {
    hash_hex[i * 2] = HEX_MAP[(key_hash[i] >> 4) & 0x0F];
    hash_hex[i * 2 + 1] = HEX_MAP[key_hash[i] & 0x0F];
  }

  // Lookup key by hash
  kj::Maybe<kj::Own<ApiKey>> result;

  {
    auto lock = state_.lockExclusive();

    KJ_IF_SOME(key_id, lock->key_hash_to_id.find(hash_hex)) {
      // Found the key ID, now get the full ApiKey
      KJ_IF_SOME(api_key, lock->keys_by_id.find(key_id)) {
        if (!api_key->revoked) {
          // Update last_used timestamp
          api_key->last_used = std::chrono::system_clock::now();

          // Return a copy of the key
          auto copy = kj::heap<ApiKey>();
          copy->key_id = kj::str(api_key->key_id);
          copy->key_hash = kj::heapArray<uint8_t>(api_key->key_hash.asPtr());
          copy->user_id = kj::str(api_key->user_id);
          copy->name = kj::str(api_key->name);
          for (auto& perm : api_key->permissions) {
            copy->permissions.add(kj::str(perm));
          }
          copy->created_at = api_key->created_at;
          copy->last_used = api_key->last_used;
          copy->revoked = api_key->revoked;

          result = kj::mv(copy);
        }
      }
    }
  }

  return result;
}

bool ApiKeyManager::revoke(kj::StringPtr key_id) {
  auto lock = state_.lockExclusive();

  KJ_IF_SOME(api_key, lock->keys_by_id.find(key_id)) {
    // Check if already revoked
    if (api_key->revoked) {
      return false;
    }

    // Mark as revoked
    api_key->revoked = true;

    // Remove from hash_to_id map to prevent future lookups
    kj::String hash_hex = kj::heapString(SHA256_HASH_BYTES * 2);
    static constexpr char HEX_MAP[] = "0123456789abcdef";
    for (size_t i = 0; i < SHA256_HASH_BYTES; ++i) {
      hash_hex[i * 2] = HEX_MAP[(api_key->key_hash[i] >> 4) & 0x0F];
      hash_hex[i * 2 + 1] = HEX_MAP[api_key->key_hash[i] & 0x0F];
    }
    lock->key_hash_to_id.erase(hash_hex);

    return true;
  }

  return false;
}

kj::Vector<kj::Own<ApiKey>> ApiKeyManager::list_keys(kj::StringPtr user_id) {
  kj::Vector<kj::Own<ApiKey>> result;

  auto lock = state_.lockExclusive();

  for (auto& entry : lock->keys_by_id) {
    if (entry.value->user_id == user_id) {
      // Create a copy of the key
      auto copy = kj::heap<ApiKey>();
      copy->key_id = kj::str(entry.value->key_id);
      copy->key_hash = kj::heapArray<uint8_t>(entry.value->key_hash.asPtr());
      copy->user_id = kj::str(entry.value->user_id);
      copy->name = kj::str(entry.value->name);
      for (auto& perm : entry.value->permissions) {
        copy->permissions.add(kj::str(perm));
      }
      copy->created_at = entry.value->created_at;
      copy->last_used = entry.value->last_used;
      copy->revoked = entry.value->revoked;

      result.add(kj::mv(copy));
    }
  }

  return result;
}

bool ApiKeyManager::has_permission(const ApiKey& key, kj::StringPtr permission) {
  for (auto& perm : key.permissions) {
    if (perm == permission) {
      return true;
    }
  }
  return false;
}

size_t ApiKeyManager::active_key_count() const {
  auto lock = state_.lockShared();

  size_t count = 0;
  for (auto& entry : lock->keys_by_id) {
    if (!entry.value->revoked) {
      ++count;
    }
  }

  return count;
}

kj::Array<uint8_t> ApiKeyManager::generate_random_bytes(size_t length) {
  kj::Array<uint8_t> bytes = kj::heapArray<uint8_t>(length);

  // Use OpenSSL RAND_bytes for cryptographically secure random bytes
  // std::random_device is not cryptographically secure
  int result = RAND_bytes(bytes.begin(), static_cast<int>(length));
  KJ_REQUIRE(result == 1, "Failed to generate random bytes");

  return bytes;
}

kj::Array<uint8_t> ApiKeyManager::sha256_hash(kj::ArrayPtr<const uint8_t> data) {
  kj::Array<uint8_t> digest = kj::heapArray<uint8_t>(SHA256_HASH_BYTES);
  unsigned int digest_len = 0;

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  KJ_REQUIRE(ctx != nullptr, "Failed to create EVP context");

  if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
    EVP_MD_CTX_free(ctx);
    KJ_FAIL_REQUIRE("Failed to initialize SHA256");
  }

  if (EVP_DigestUpdate(ctx, data.begin(), data.size()) != 1) {
    EVP_MD_CTX_free(ctx);
    KJ_FAIL_REQUIRE("Failed to update SHA256 digest");
  }

  if (EVP_DigestFinal_ex(ctx, digest.begin(), &digest_len) != 1) {
    EVP_MD_CTX_free(ctx);
    KJ_FAIL_REQUIRE("Failed to finalize SHA256 digest");
  }

  EVP_MD_CTX_free(ctx);

  KJ_REQUIRE(digest_len == SHA256_HASH_BYTES, "Unexpected SHA256 digest length");

  return digest;
}

kj::String ApiKeyManager::generate_key_id() {
  auto lock = state_.lockExclusive();

  // Generate a unique 16-character hex key ID
  // Combine counter with random bytes for uniqueness
  uint64_t counter = lock->next_key_counter++;
  uint64_t rand_val;
  RAND_bytes(reinterpret_cast<unsigned char*>(&rand_val), sizeof(rand_val));

  static constexpr char HEX_MAP[] = "0123456789abcdef";
  kj::String result = kj::heapString(KEY_ID_LENGTH);

  // Mix counter and random for the ID
  for (size_t i = 0; i < KEY_ID_LENGTH; ++i) {
    uint8_t byte;
    if (i < 8) {
      byte = (counter >> (i * 8)) & 0xFF;
    } else {
      byte = (rand_val >> ((i - 8) * 8)) & 0xFF;
    }
    result[i] = HEX_MAP[byte & 0x0F];
  }

  return result;
}

} // namespace veloz::gateway
