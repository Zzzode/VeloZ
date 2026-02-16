#pragma once

#include <kj/array.h>
#include <kj/string.h>
#include <string>   // std::string for OpenSSL compatibility
#include <string_view>

namespace veloz::exec {

/**
 * @brief HMAC-SHA256 signature generator wrapper
 *
 * This wrapper encapsulates OpenSSL HMAC functionality for generating HMAC-SHA256 signatures
 * required for exchange API authentication. It provides a KJ-style interface and handles
 * the underlying OpenSSL C API calls safely.
 *
 * This is required because KJ does not provide HMAC functionality, and exchange APIs
 * require HMAC-SHA256 signatures for authentication.
 */
class HmacSha256 {
public:
  /**
   * @brief Generate HMAC-SHA256 signature (KJ version)
   * @param key Secret key for HMAC
   * @param data Data to sign
   * @return Hex-encoded signature string
   */
  [[nodiscard]] static kj::String sign(kj::StringPtr key, kj::StringPtr data);

  /**
   * @brief Generate HMAC-SHA256 signature (std::string version for OpenSSL compatibility)
   * @param key Secret key for HMAC (std::string for OpenSSL compatibility)
   * @param data Data to sign (std::string for OpenSSL compatibility)
   * @return Hex-encoded signature string
   */
  [[nodiscard]] static std::string sign(const std::string& key, const std::string& data);

private:
  HmacSha256() = delete;
  ~HmacSha256() = delete;
};

} // namespace veloz::exec
