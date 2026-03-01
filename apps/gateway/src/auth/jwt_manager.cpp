#include "jwt_manager.h"

#include <chrono>
#include <kj/debug.h>
#include <kj/encoding.h>
#include <kj/exception.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <random>

// Include yyjson for JSON parsing/building
extern "C" {
#include <yyjson.h>
}

namespace veloz::gateway::auth {

// =============================================================================
// Utility Functions
// =============================================================================

namespace {

// Convert Base64URL encoded string to standard Base64 for decoding.
// Base64URL uses '-' and '_' instead of '+' and '/', and omits padding.
kj::Array<kj::byte> decodeBase64Url(kj::ArrayPtr<const char> encoded) {
  // First, convert Base64URL to standard Base64
  kj::Vector<char> standardB64;
  standardB64.reserve(encoded.size() + 4);

  for (char c : encoded) {
    if (c == '-') {
      standardB64.add('+');
    } else if (c == '_') {
      standardB64.add('/');
    } else {
      standardB64.add(c);
    }
  }

  // Add padding if needed (Base64 requires length to be multiple of 4)
  while (standardB64.size() % 4 != 0) {
    standardB64.add('=');
  }

  standardB64.add('\0');

  // Now decode using standard Base64
  kj::StringPtr b64Str(standardB64.begin(), standardB64.size() - 1);
  auto result = kj::decodeBase64(b64Str);
  if (result.hadErrors) {
    return kj::heapArray<kj::byte>(0);
  }
  return kj::mv(result);
}

} // namespace

kj::String create_header_json(kj::StringPtr algorithm) {
  // Create JWT header: {"alg":"HS256","typ":"JWT"}
  return kj::str("{\"alg\":\"", algorithm, "\",\"typ\":\"JWT\"}");
}

kj::String generate_random_string(size_t length) {
  // Thread-safe random device and generator
  // Each thread gets its own generator to avoid synchronization overhead
  thread_local std::random_device rd;
  thread_local std::mt19937_64 gen(rd());
  thread_local std::uniform_int_distribution<uint64_t> dis;

  // Each 64-bit value produces 16 hex characters
  kj::Vector<char> result;
  size_t remaining = length;

  while (remaining > 0) {
    uint64_t value = dis(gen);
    char buf[17];
    snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(value));

    size_t to_copy = std::min(remaining * 2, static_cast<size_t>(16));
    for (size_t i = 0; i < to_copy; ++i) {
      result.add(buf[i]);
    }
    remaining -= to_copy / 2;
  }
  result.add('\0');

  return kj::String(result.releaseAsArray());
}

kj::Maybe<kj::String> extract_jti(kj::StringPtr token) {
  // Find two dots
  size_t first_dot = token.findFirst('.').orDefault(SIZE_MAX);
  size_t second_dot = SIZE_MAX;

  // Find second dot by slicing from after the first dot
  if (first_dot != SIZE_MAX && first_dot + 1 < token.size()) {
    KJ_IF_SOME(offset, token.slice(first_dot + 1).findFirst('.')) {
      second_dot = first_dot + 1 + offset;
    }
  }

  if (first_dot == SIZE_MAX || second_dot == SIZE_MAX) {
    return kj::none;
  }

  // Extract payload
  kj::ArrayPtr<const char> payload_chars = token.slice(first_dot + 1, second_dot);

  // Decode Base64URL
  kj::Array<kj::byte> payload_bytes = decodeBase64Url(payload_chars);
  if (payload_bytes.size() == 0) {
    return kj::none;
  }

  // Parse JSON
  yyjson_doc* doc =
      yyjson_read(reinterpret_cast<const char*>(payload_bytes.begin()), payload_bytes.size(), 0);
  if (!doc) {
    return kj::none;
  }
  KJ_DEFER(yyjson_doc_free(doc));

  yyjson_val* root = yyjson_doc_get_root(doc);
  yyjson_val* jti_val = yyjson_obj_get(root, "jti");

  if (!jti_val || !yyjson_is_str(jti_val)) {
    return kj::none;
  }

  return kj::str(yyjson_get_str(jti_val));
}

// =============================================================================
// JwtManager Implementation
// =============================================================================

JwtManager::JwtManager(kj::StringPtr secret, kj::Maybe<kj::StringPtr> refresh_secret,
                       uint32_t access_expiry_seconds, uint32_t refresh_expiry_seconds)
    : secret_(kj::str(secret)),
      refresh_secret_(refresh_secret.map([](kj::StringPtr rs) { return kj::str(rs); })
                          .orDefault(kj::str(secret))),
      access_expiry_seconds_(access_expiry_seconds),
      refresh_expiry_seconds_(refresh_expiry_seconds) {
  // Initialize last_error_ to NONE
  {
    auto lock = last_error_.lockExclusive();
    *lock = JwtError::NONE;
  }
  // Security note: ensure secret is long enough
  KJ_REQUIRE(secret.size() >= 32, "JWT secret should be at least 32 bytes for security");
}

JwtManager::~JwtManager() = default;

kj::String JwtManager::generate_jti() {
  // Generate 128-bit random JTI (32 hex characters)
  return generate_random_string(16);
}

int64_t JwtManager::current_timestamp() const {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

kj::String JwtManager::create_payload_json(kj::StringPtr user_id,
                                           kj::Maybe<kj::StringPtr> api_key_id, int64_t iat,
                                           int64_t exp, kj::Maybe<kj::StringPtr> jti,
                                           kj::StringPtr type) {
  // Build JSON payload using KJ string concatenation
  kj::String api_key_part;
  KJ_IF_SOME(aid, api_key_id) {
    api_key_part = kj::str(",\"api_key_id\":\"", aid, "\"");
  }

  kj::String jti_part;
  KJ_IF_SOME(j, jti) {
    jti_part = kj::str(",\"jti\":\"", j, "\"");
  }

  return kj::str("{\"sub\":\"", user_id, "\",\"iat\":", iat, ",\"exp\":", exp, ",\"type\":\"", type,
                 "\"", api_key_part, jti_part, "}");
}

kj::String JwtManager::create_access_token(kj::StringPtr user_id,
                                           kj::Maybe<kj::StringPtr> api_key_id) {
  // Create header
  kj::String header_json = create_header_json("HS256");

  // Create payload
  int64_t iat = current_timestamp();
  int64_t exp = iat + access_expiry_seconds_;
  kj::String payload_json =
      create_payload_json(user_id, api_key_id, iat, exp, kj::none, "access"_kj);

  // Encode header and payload
  kj::String header_b64 = base64url_encode(header_json.asBytes());
  kj::String payload_b64 = base64url_encode(payload_json.asBytes());

  // Create signing input: header.payload
  kj::String signing_input = kj::str(header_b64, ".", payload_b64);

  // Generate signature
  kj::Array<kj::byte> signature = hmac_sha256(
      reinterpret_cast<const unsigned char*>(secret_.cStr()), secret_.size(),
      reinterpret_cast<const unsigned char*>(signing_input.cStr()), signing_input.size());

  // Encode signature
  kj::String signature_b64 = base64url_encode(signature.asPtr());

  // Return complete token
  return kj::str(signing_input, ".", signature_b64);
}

kj::String JwtManager::create_refresh_token(kj::StringPtr user_id) {
  // Create header
  kj::String header_json = create_header_json("HS256");

  // Create payload with JTI
  int64_t iat = current_timestamp();
  int64_t exp = iat + refresh_expiry_seconds_;
  kj::String jti_str = generate_jti();
  kj::StringPtr jti_ref = jti_str;
  kj::String payload_json = create_payload_json(user_id, kj::none, iat, exp, jti_ref, "refresh"_kj);

  // Encode header and payload
  kj::String header_b64 = base64url_encode(header_json.asBytes());
  kj::String payload_b64 = base64url_encode(payload_json.asBytes());

  // Create signing input: header.payload
  kj::String signing_input = kj::str(header_b64, ".", payload_b64);

  // Generate signature using refresh secret
  kj::Array<kj::byte> signature = hmac_sha256(
      reinterpret_cast<const unsigned char*>(refresh_secret_.cStr()), refresh_secret_.size(),
      reinterpret_cast<const unsigned char*>(signing_input.cStr()), signing_input.size());

  // Encode signature
  kj::String signature_b64 = base64url_encode(signature.asPtr());

  // Return complete token
  return kj::str(signing_input, ".", signature_b64);
}

kj::Maybe<TokenInfo> JwtManager::verify_access_token(kj::StringPtr token) {
  return verify_token(token, secret_, false);
}

kj::Maybe<TokenInfo> JwtManager::verify_refresh_token(kj::StringPtr token) {
  return verify_token(token, refresh_secret_, true);
}

kj::Maybe<TokenInfo> JwtManager::verify_token(kj::StringPtr token, kj::StringPtr expected_secret,
                                              bool check_revocation) {
  set_last_error(JwtError::NONE);

  // Find two dots in the token
  size_t first_dot = token.findFirst('.').orDefault(SIZE_MAX);
  size_t second_dot = SIZE_MAX;

  // Find second dot by slicing from after the first dot
  if (first_dot != SIZE_MAX && first_dot + 1 < token.size()) {
    KJ_IF_SOME(offset, token.slice(first_dot + 1).findFirst('.')) {
      second_dot = first_dot + 1 + offset;
    }
  }

  if (first_dot == SIZE_MAX || second_dot == SIZE_MAX || first_dot == 0 ||
      second_dot == first_dot + 1 || second_dot == token.size() - 1) {
    set_last_error(JwtError::INVALID_FORMAT);
    return kj::none;
  }

  // Extract parts
  kj::ArrayPtr<const char> header_chars = token.slice(0, first_dot);
  kj::ArrayPtr<const char> payload_chars = token.slice(first_dot + 1, second_dot);
  kj::ArrayPtr<const char> signature_chars = token.slice(second_dot + 1);

  // Decode header
  kj::Array<kj::byte> header_decode = decodeBase64Url(header_chars);
  if (header_decode.size() == 0) {
    set_last_error(JwtError::INVALID_BASE64);
    return kj::none;
  }

  // Parse header JSON
  yyjson_doc* header_doc =
      yyjson_read(reinterpret_cast<const char*>(header_decode.begin()), header_decode.size(), 0);
  if (!header_doc) {
    set_last_error(JwtError::INVALID_JSON);
    return kj::none;
  }
  KJ_DEFER(yyjson_doc_free(header_doc));

  yyjson_val* header_root = yyjson_doc_get_root(header_doc);
  yyjson_val* alg_val = yyjson_obj_get(header_root, "alg");

  if (!alg_val || !yyjson_is_str(alg_val)) {
    set_last_error(JwtError::MISSING_CLAIMS);
    return kj::none;
  }

  // Verify algorithm is HS256
  const char* alg = yyjson_get_str(alg_val);
  if (strcmp(alg, "HS256") != 0) {
    set_last_error(JwtError::ALGORITHM_MISMATCH);
    return kj::none;
  }

  // Decode payload
  kj::Array<kj::byte> payload_decode = decodeBase64Url(payload_chars);
  if (payload_decode.size() == 0) {
    set_last_error(JwtError::INVALID_BASE64);
    return kj::none;
  }

  // Parse payload JSON
  yyjson_doc* payload_doc =
      yyjson_read(reinterpret_cast<const char*>(payload_decode.begin()), payload_decode.size(), 0);
  if (!payload_doc) {
    set_last_error(JwtError::INVALID_JSON);
    return kj::none;
  }
  KJ_DEFER(yyjson_doc_free(payload_doc));

  yyjson_val* payload_root = yyjson_doc_get_root(payload_doc);

  // Extract required claims
  yyjson_val* sub_val = yyjson_obj_get(payload_root, "sub");
  yyjson_val* iat_val = yyjson_obj_get(payload_root, "iat");
  yyjson_val* exp_val = yyjson_obj_get(payload_root, "exp");

  if (!sub_val || !yyjson_is_str(sub_val) || !iat_val || !yyjson_is_num(iat_val) || !exp_val ||
      !yyjson_is_num(exp_val)) {
    set_last_error(JwtError::MISSING_CLAIMS);
    return kj::none;
  }

  // Extract claim values
  kj::String user_id = kj::str(yyjson_get_str(sub_val));
  int64_t iat = yyjson_get_sint(iat_val);
  int64_t exp = yyjson_get_sint(exp_val);

  // Optional api_key_id
  kj::Maybe<kj::String> api_key_id = kj::none;
  yyjson_val* api_key_val = yyjson_obj_get(payload_root, "api_key_id");
  if (api_key_val && yyjson_is_str(api_key_val)) {
    api_key_id = kj::str(yyjson_get_str(api_key_val));
  }

  // Check expiration
  int64_t now = current_timestamp();
  if (exp <= now) {
    set_last_error(JwtError::EXPIRED);
    return kj::none;
  }

  // Check iat is not in the future (allow 60 seconds clock skew)
  if (iat > now + 60) {
    set_last_error(JwtError::FUTURE_ISSUED);
    return kj::none;
  }

  // Check revocation for refresh tokens
  if (check_revocation) {
    yyjson_val* jti_val = yyjson_obj_get(payload_root, "jti");
    if (jti_val && yyjson_is_str(jti_val)) {
      kj::String jti = kj::str(yyjson_get_str(jti_val));
      auto locked = revoked_jtis_.lockShared();
      if (locked->contains(jti)) {
        set_last_error(JwtError::REVOKED);
        return kj::none;
      }
    }
  }

  // Verify signature
  kj::String signing_input = kj::str(header_chars, ".", payload_chars);

  kj::Array<kj::byte> signature_decode = decodeBase64Url(signature_chars);
  if (signature_decode.size() == 0) {
    set_last_error(JwtError::INVALID_BASE64);
    return kj::none;
  }

  if (!verify_hmac_sha256(expected_secret, signing_input.asBytes(), signature_decode.asPtr())) {
    set_last_error(JwtError::INVALID_SIGNATURE);
    return kj::none;
  }

  // Return token info
  return TokenInfo(kj::mv(user_id), kj::mv(api_key_id), iat, exp);
}

void JwtManager::revoke_refresh_token(kj::StringPtr jti) {
  auto locked = revoked_jtis_.lockExclusive();
  locked->insert(kj::str(jti));
}

kj::Maybe<kj::String> JwtManager::extract_payload(kj::StringPtr token) {
  // Find two dots
  size_t first_dot = token.findFirst('.').orDefault(SIZE_MAX);
  size_t second_dot = SIZE_MAX;

  // Find second dot by slicing from after the first dot
  if (first_dot != SIZE_MAX && first_dot + 1 < token.size()) {
    KJ_IF_SOME(offset, token.slice(first_dot + 1).findFirst('.')) {
      second_dot = first_dot + 1 + offset;
    }
  }

  if (first_dot == SIZE_MAX || second_dot == SIZE_MAX) {
    return kj::none;
  }

  kj::ArrayPtr<const char> payload_chars = token.slice(first_dot + 1, second_dot);
  kj::Array<kj::byte> decode_result = decodeBase64Url(payload_chars);
  if (decode_result.size() == 0) {
    return kj::none;
  }

  // Use heapString with proper byte array (handles NUL termination internally)
  return kj::heapString(decode_result.asChars());
}

JwtError JwtManager::get_last_error() const {
  auto lock = last_error_.lockShared();
  return *lock;
}

size_t JwtManager::get_revoked_count() const {
  auto locked = revoked_jtis_.lockShared();
  return locked->size();
}

void JwtManager::clear_revoked_tokens() {
  auto locked = revoked_jtis_.lockExclusive();
  locked->clear();
}

void JwtManager::cleanup_old_revoked_tokens(int64_t before_timestamp) {
  // TODO: Implement this with timestamp tracking
  // For now, this is a no-op
  (void)before_timestamp;
}

// =============================================================================
// Cryptographic Functions
// =============================================================================

kj::Array<kj::byte> JwtManager::base64url_decode(kj::StringPtr encoded) {
  // Use our Base64URL decoder (handles URL-safe characters and padding)
  return ::veloz::gateway::auth::decodeBase64Url(encoded.asArray());
}

kj::String JwtManager::base64url_encode(kj::ArrayPtr<const kj::byte> data) {
  // Use KJ's built-in Base64URL encoder
  return kj::encodeBase64Url(data);
}

kj::Array<kj::byte> JwtManager::hmac_sha256(const unsigned char* key, size_t key_len,
                                            const unsigned char* data, size_t data_len) {
  // Allocate buffer for HMAC digest
  kj::Array<kj::byte> digest = kj::heapArray<kj::byte>(EVP_MAX_MD_SIZE);
  unsigned int digest_len = 0;

  // Generate HMAC using OpenSSL
  HMAC(EVP_sha256(), key, static_cast<int>(key_len), data, data_len, digest.begin(), &digest_len);

  // Resize to actual length
  return digest.slice(0, digest_len).attach(kj::mv(digest));
}

bool JwtManager::verify_hmac_sha256(kj::StringPtr key, kj::ArrayPtr<const kj::byte> data,
                                    kj::ArrayPtr<const kj::byte> signature) {
  // Compute expected signature
  kj::Array<kj::byte> expected = hmac_sha256(reinterpret_cast<const unsigned char*>(key.cStr()),
                                             key.size(), data.begin(), data.size());

  // Constant-time comparison to prevent timing attacks
  if (expected.size() != signature.size()) {
    return false;
  }

  unsigned char result = 0;
  for (size_t i = 0; i < expected.size(); ++i) {
    result |= expected[i] ^ signature[i];
  }

  return result == 0;
}

void JwtManager::set_last_error(JwtError error) const {
  auto lock = last_error_.lockExclusive();
  *lock = error;
}

} // namespace veloz::gateway::auth
