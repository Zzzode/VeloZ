#include "veloz/exec/hmac_wrapper.h"

#include <kj/string.h>

#ifndef VELOZ_NO_OPENSSL
#include <openssl/evp.h>
#include <openssl/hmac.h>
#endif

namespace veloz::exec {

kj::String HmacSha256::sign(kj::StringPtr key, kj::StringPtr data) {
#ifdef VELOZ_NO_OPENSSL
  // Placeholder for builds without OpenSSL
  // In production, this should be implemented with a proper crypto library
  return kj::str("");
#else
  // Use std::string internally for OpenSSL API compatibility
  std::string key_str(key.cStr(), key.size());
  std::string data_str(data.cStr(), data.size());

  std::string signature = sign(key_str, data_str);

  // Convert to KJ String - build from std::string data and length
  kj::String result(kj::heapString(signature.data(), signature.size()));
  return result;
#endif
}

std::string HmacSha256::sign(const std::string& key, const std::string& data) {
#ifdef VELOZ_NO_OPENSSL
  // Placeholder for builds without OpenSSL
  // In production, this should be implemented with a proper crypto library
  (void)key;
  (void)data;
  return "";
#else
  // HMAC-SHA256 signature generation using OpenSSL
  // KJ does not provide HMAC functionality, so we use OpenSSL for exchange API authentication
  // This is acceptable per CLAUDE.md guidelines for external API compatibility

  // Allocate buffer for HMAC digest
  kj::Array<uint8_t> digest_buffer = kj::heapArray<uint8_t>(EVP_MAX_MD_SIZE);
  unsigned int digest_len = 0;

  // Generate HMAC
  HMAC(EVP_sha256(), key.c_str(), static_cast<int>(key.length()),
       reinterpret_cast<const unsigned char*>(data.c_str()), data.length(), digest_buffer.begin(),
       &digest_len);

  // Hex encode signature
  std::string signature;
  signature.reserve(digest_len * 2);
  static const char hex_chars[] = "0123456789abcdef";
  for (unsigned int i = 0; i < digest_len; ++i) {
    signature.push_back(hex_chars[(digest_buffer[i] >> 4) & 0x0F]);
    signature.push_back(hex_chars[digest_buffer[i] & 0x0F]);
  }

  return signature;
#endif
}

} // namespace veloz::exec
