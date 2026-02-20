#include "veloz/exec/hmac_wrapper.h"

#include <kj/string.h>

namespace veloz::exec {

kj::String hex_encode(kj::ArrayPtr<const uint8_t> data) {
  static constexpr char HEX_MAP[] = "0123456789abcdef";

  kj::String result = kj::heapString(data.size() * 2);
  char* out = result.begin();

  for (size_t i = 0; i < data.size(); ++i) {
    uint8_t byte = data[i];
    out[i * 2] = HEX_MAP[(byte >> 4) & 0x0F];
    out[i * 2 + 1] = HEX_MAP[byte & 0x0F];
  }

  return result;
}

kj::String HmacSha256::sign(kj::StringPtr key, kj::StringPtr data) {
  // Allocate buffer for HMAC digest
  kj::Array<uint8_t> digest_buffer = kj::heapArray<uint8_t>(EVP_MAX_MD_SIZE);
  unsigned int digest_len = 0;

  // Generate HMAC using OpenSSL
  // OpenSSL API requires const char* - we use .cStr() from KJ strings
  // The returned pointers remain valid as long as the KJ String objects are in scope
  HMAC(EVP_sha256(), key.cStr(), static_cast<int>(key.size()),
       reinterpret_cast<const unsigned char*>(data.cStr()), static_cast<int>(data.size()),
       digest_buffer.begin(), &digest_len);

  // Create hex-encoded result
  kj::ArrayPtr<const uint8_t> digest_ptr = digest_buffer.slice(0, static_cast<size_t>(digest_len));
  return hex_encode(digest_ptr);
}

} // namespace veloz::exec
