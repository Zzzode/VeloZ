#pragma once

#include <kj/array.h>
#include <kj/string.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace veloz::exec {

/**
 * @brief Helper function for hex encoding bytes to string
 *
 * Converts an array of bytes to a hex-encoded KJ string.
 * This is needed for HMAC signature encoding.
 *
 * @param data Array of bytes to encode
 * @return Hex-encoded string
 */
kj::String hex_encode(kj::ArrayPtr<const uint8_t> data);

/**
 * @brief HMAC-SHA256 signature generator wrapper
 *
 * This wrapper encapsulates OpenSSL HMAC functionality for generating HMAC-SHA256 signatures
 * required for exchange API authentication. It uses only KJ types for the interface.
 *
 * This is required because KJ does not provide HMAC functionality, and exchange APIs
 * require HMAC-SHA256 signatures for authentication.
 */
class HmacSha256 {
public:
  /**
   * @brief Generate HMAC-SHA256 signature
   * @param key Secret key for HMAC (as KJ string pointer)
   * @param data Data to sign (as KJ string pointer)
   * @return Hex-encoded signature string
   */
  [[nodiscard]] static kj::String sign(kj::StringPtr key, kj::StringPtr data);

private:
  HmacSha256() = delete;
  ~HmacSha256() = delete;
};

} // namespace veloz::exec
