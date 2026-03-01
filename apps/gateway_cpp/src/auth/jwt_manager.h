/**
 * @file jwt_manager.h
 * @brief JWT authentication and token management for the Gateway
 *
 * This module provides secure JWT (JSON Web Token) creation and verification
 * using HMAC-SHA256 signatures. It supports both access tokens (short-lived)
 * and refresh tokens (long-lived with revocation tracking).
 *
 * Design decisions:
 * - Uses OpenSSL EVP API for HMAC-SHA256 (SIMD-optimized)
 * - Access tokens: 1-hour expiry by default
 * - Refresh tokens: 7-day expiry with JTI (JWT ID) for revocation
 * - Token revocation using lock-free HashSet for thread safety
 * - Base64URL encoding/decoding for JWT compatibility
 */

#pragma once

#include <cstdint>
#include <kj/array.h>
#include <kj/common.h>
#include <kj/map.h>
#include <kj/mutex.h>
#include <kj/one-of.h>
#include <kj/string.h>
#include <kj/table.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace veloz::gateway::auth {

/**
 * @brief JWT token payload information
 *
 * Contains the decoded information from a verified JWT token.
 * All timestamps are in Unix seconds since epoch.
 */
struct TokenInfo {
  kj::String user_id;
  // User identifier (e.g., "user_123")

  kj::Maybe<kj::String> api_key_id;
  // Optional API key identifier if token was issued for an API key

  int64_t issued_at;
  // Unix timestamp when token was issued (iat claim)

  int64_t expires_at;
  // Unix timestamp when token expires (exp claim)

  TokenInfo() : issued_at(0), expires_at(0) {}

  TokenInfo(kj::String user_id_param, kj::Maybe<kj::String> api_key_id_param,
            int64_t issued_at_param, int64_t expires_at_param)
      : user_id(kj::mv(user_id_param)), api_key_id(kj::mv(api_key_id_param)),
        issued_at(issued_at_param), expires_at(expires_at_param) {}

  // Move-only semantics (kj::String is not copyable)
  TokenInfo(const TokenInfo&) = delete;
  TokenInfo& operator=(const TokenInfo&) = delete;
  TokenInfo(TokenInfo&& other) noexcept = default;
  TokenInfo& operator=(TokenInfo&& other) noexcept = default;
};

/**
 * @brief Token type enumeration
 */
enum class TokenType {
  ACCESS,
  // Short-lived token for API access (default 1 hour)

  REFRESH
  // Long-lived token for obtaining new access tokens (default 7 days)
};

/**
 * @brief JWT error codes
 *
 * Describes specific failure modes during token verification.
 */
enum class JwtError {
  NONE,
  // No error - token is valid

  INVALID_FORMAT,
  // Token does not follow JWT format (header.payload.signature)

  INVALID_BASE64,
  // Token contains invalid Base64URL encoding

  INVALID_JSON,
  // Token payload is not valid JSON

  EXPIRED,
  // Token has expired (exp claim in the past)

  FUTURE_ISSUED,
  // Token was issued in the future (iat claim > current time)

  INVALID_SIGNATURE,
  // HMAC signature does not match

  MISSING_CLAIMS,
  // Token is missing required claims (exp, iat, sub)

  REVOKED,
  // Refresh token has been revoked

  ALGORITHM_MISMATCH
  // Token uses unsupported algorithm (must be HS256)
};

/**
 * @brief JWT manager for token creation and verification
 *
 * This class handles:
 * - Creating signed JWT tokens with HMAC-SHA256
 * - Verifying token signatures and validating claims
 * - Managing refresh token revocation
 * - Base64URL encoding/decoding
 *
 * Thread safety: All public methods are thread-safe using internal mutex.
 *
 * Performance target: <20μs per token verification
 */
class JwtManager {
public:
  /**
   * @brief Construct JWT manager with secret keys
   *
   * @param secret Secret key for HMAC-SHA256 signing (access tokens)
   * @param refresh_secret Optional separate secret for refresh tokens.
   *                    If not provided, uses the same secret as access tokens.
   *                    Using separate keys provides better security isolation.
   * @param access_expiry_seconds Expiry time for access tokens in seconds (default: 3600 = 1 hour)
   * @param refresh_expiry_seconds Expiry time for refresh tokens in seconds (default: 604800 = 7
   * days)
   *
   * @note Secrets must be at least 32 bytes for secure HMAC-SHA256.
   *       Shorter secrets will still work but reduce security.
   */
  explicit JwtManager(kj::StringPtr secret, kj::Maybe<kj::StringPtr> refresh_secret = kj::none,
                      uint32_t access_expiry_seconds = 3600,
                      uint32_t refresh_expiry_seconds = 604800);

  /**
   * @brief Destructor - cleans up OpenSSL resources if needed
   */
  ~JwtManager();

  // Non-copyable, non-movable (contains mutex)
  JwtManager(const JwtManager&) = delete;
  JwtManager& operator=(const JwtManager&) = delete;
  JwtManager(JwtManager&&) = delete;
  JwtManager& operator=(JwtManager&&) = delete;

  /**
   * @brief Create an access token for a user
   *
   * @param user_id User identifier (e.g., "user_123")
   * @param api_key_id Optional API key identifier
   * @return JWT access token string
   *
   * The token includes claims:
   * - sub (subject): user_id
   * - api_key_id (optional): API key identifier
   * - iat (issued at): current Unix timestamp
   * - exp (expires): current + access_expiry_seconds
   * - type: "access"
   */
  [[nodiscard]] kj::String create_access_token(kj::StringPtr user_id,
                                               kj::Maybe<kj::StringPtr> api_key_id = kj::none);

  /**
   * @brief Create a refresh token for a user
   *
   * @param user_id User identifier
   * @return JWT refresh token string
   *
   * The token includes claims:
   * - sub (subject): user_id
   * - jti (JWT ID): unique identifier for revocation
   * - iat (issued at): current Unix timestamp
   * - exp (expires): current + refresh_expiry_seconds
   * - type: "refresh"
   *
   * @note The JTI is tracked in a revocation set. Use revoke_refresh_token() to invalidate.
   */
  [[nodiscard]] kj::String create_refresh_token(kj::StringPtr user_id);

  /**
   * @brief Verify an access token
   *
   * @param token JWT access token string
   * @return TokenInfo if valid, kj::none if verification fails
   *
   * Verification checks:
   * 1. Token format (header.payload.signature)
   * 2. Base64URL encoding validity
   * 3. JSON payload parsing
   * 4. Required claims presence (exp, iat, sub)
   * 5. Expiration (exp > current time)
   * 6. Issue time (iat <= current time)
   * 7. HMAC signature verification
   * 8. Algorithm must be HS256
   *
   * Use get_last_error() to retrieve specific error if verification fails.
   */
  [[nodiscard]] kj::Maybe<TokenInfo> verify_access_token(kj::StringPtr token);

  /**
   * @brief Verify a refresh token
   *
   * @param token JWT refresh token string
   * @return TokenInfo if valid, kj::none if verification fails
   *
   * Verification includes all checks from verify_access_token() plus:
   * - JTI (JWT ID) is not in the revoked set
   */
  [[nodiscard]] kj::Maybe<TokenInfo> verify_refresh_token(kj::StringPtr token);

  /**
   * @brief Revoke a refresh token by its JTI
   *
   * @param jti JWT ID from the refresh token
   *
   * This adds the JTI to the revocation set. Once revoked,
   * any attempt to verify this refresh token will fail.
   *
   * @note Revoked JTIs are stored in a HashSet. In production,
   *       consider periodic cleanup of old JTIs to prevent
   *       unbounded memory growth.
   */
  void revoke_refresh_token(kj::StringPtr jti);

  /**
   * @brief Extract payload from token without verifying signature
   *
   * @param token JWT token string
   * @return JSON payload string, or kj::none if parsing fails
   *
   * This is useful for logging or debugging. Do not use
   * extracted payload for authorization - always verify the signature.
   */
  [[nodiscard]] kj::Maybe<kj::String> extract_payload(kj::StringPtr token);

  /**
   * @brief Get the last error code from verification
   *
   * @return JwtError describing the last verification failure
   *
   * After any verify_*() call that returns kj::none, this
   * method can be used to determine the specific reason.
   */
  [[nodiscard]] JwtError get_last_error() const;

  /**
   * @brief Get the count of revoked refresh tokens
   *
   * @return Number of JTIs in the revocation set
   *
   * This can be used for monitoring and cleanup decisions.
   */
  [[nodiscard]] size_t get_revoked_count() const;

  /**
   * @brief Clear all revoked refresh tokens
   *
   * Use this to clean up the revocation set.
   * Only call this when you're sure all old tokens
   * have expired, or use periodic cleanup based on expiry time.
   */
  void clear_revoked_tokens();

  /**
   * @brief Clean up revoked tokens older than specified time
   *
   * @param before_timestamp Unix timestamp; tokens revoked before this time are removed
   *
   * @note This requires storing revocation timestamps, which is not
   *       implemented in the current HashSet-based approach.
   *       This method is a placeholder for future enhancement.
   */
  void cleanup_old_revoked_tokens(int64_t before_timestamp);

private:
  // Secret keys for HMAC signing
  kj::String secret_;
  kj::String refresh_secret_;

  // Token expiry durations
  uint32_t access_expiry_seconds_;
  uint32_t refresh_expiry_seconds_;

  // Revoked refresh token JTIs (thread-safe)
  mutable kj::MutexGuarded<kj::HashSet<kj::String>> revoked_jtis_;

  // Last error from verification (thread-safe)
  mutable kj::MutexGuarded<JwtError> last_error_;

  /**
   * @brief Generate a unique JWT ID (JTI)
   *
   * @return Unique string identifier
   *
   * Uses 128-bit random values encoded as hex.
   */
  [[nodiscard]] kj::String generate_jti();

  /**
   * @brief Get current Unix timestamp in seconds
   *
   * @return Current time as seconds since epoch
   */
  [[nodiscard]] int64_t current_timestamp() const;

  /**
   * @brief Create JWT payload JSON string
   *
   * @param user_id User identifier
   * @param api_key_id Optional API key ID
   * @param iat Issued at timestamp
   * @param exp Expires timestamp
   * @param jti Optional JWT ID (for refresh tokens)
   * @param type Token type string
   * @return JSON string representation of payload
   */
  [[nodiscard]] kj::String create_payload_json(kj::StringPtr user_id,
                                               kj::Maybe<kj::StringPtr> api_key_id, int64_t iat,
                                               int64_t exp, kj::Maybe<kj::StringPtr> jti = kj::none,
                                               kj::StringPtr type = "access"_kj);

  /**
   * @brief Verify JWT token signature and claims
   *
   * @param token JWT token string
   * @param expected_secret Secret key to verify against
   * @param check_revocation Whether to check JTI against revoked set (for refresh tokens)
   * @return TokenInfo if valid, kj::none otherwise
   */
  [[nodiscard]] kj::Maybe<TokenInfo>
  verify_token(kj::StringPtr token, kj::StringPtr expected_secret, bool check_revocation = false);

  /**
   * @brief Decode Base64URL string to bytes
   *
   * @param encoded Base64URL encoded string
   * @return Decoded byte array, or empty array on error
   */
  [[nodiscard]] kj::Array<kj::byte> base64url_decode(kj::StringPtr encoded);

  /**
   * @brief Encode bytes to Base64URL string
   *
   * @param data Byte array to encode
   * @return Base64URL encoded string
   */
  [[nodiscard]] kj::String base64url_encode(kj::ArrayPtr<const kj::byte> data);

  /**
   * @brief Generate HMAC-SHA256 signature
   *
   * @param key Secret key
   * @param data Data to sign
   * @param key_len Length of key
   * @param data_len Length of data
   * @return HMAC signature as byte array
   */
  [[nodiscard]] kj::Array<kj::byte> hmac_sha256(const unsigned char* key, size_t key_len,
                                                const unsigned char* data, size_t data_len);

  /**
   * @brief Verify HMAC-SHA256 signature
   *
   * @param key Secret key
   * @param data Original data
   * @param signature Signature to verify
   * @return true if signature is valid
   */
  [[nodiscard]] bool verify_hmac_sha256(kj::StringPtr key, kj::ArrayPtr<const kj::byte> data,
                                        kj::ArrayPtr<const kj::byte> signature);

  /**
   * @brief Set the last error code
   *
   * @param error Error code to set
   */
  void set_last_error(JwtError error) const;
};

/**
 * @brief Create a JWT header JSON string
 *
 * @param algorithm Algorithm name (typically "HS256")
 * @return JSON string representation of header
 */
[[nodiscard]] kj::String create_header_json(kj::StringPtr algorithm = "HS256"_kj);

/**
 * @brief Extract JTI from a token without verification
 *
 * @param token JWT token string
 * @return JTI string if found, kj::none otherwise
 *
 * @warning This does not verify the signature. Use only for
 *          revocation operations on tokens that have already been
 *          verified or are being revoked by admin action.
 */
[[nodiscard]] kj::Maybe<kj::String> extract_jti(kj::StringPtr token);

/**
 * @brief Generate a cryptographically secure random string
 *
 * @param length Length of the random string in bytes
 * @return Hex-encoded random string
 */
[[nodiscard]] kj::String generate_random_string(size_t length);

} // namespace veloz::gateway::auth
