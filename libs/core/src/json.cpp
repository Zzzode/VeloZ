#include "veloz/core/json.h"

#include <cstring>
#include <fstream>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/exception.h>
#include <kj/memory.h>
#include <kj/string.h>

// Include yyjson C header
#include <yyjson.h>

namespace veloz {
namespace core {

// ============================================================================
// YyJsonDoc Implementation
// ============================================================================

YyJsonDoc::~YyJsonDoc() noexcept {
  if (doc_) {
    yyjson_doc_free(doc_);
  }
}

void YyJsonDoc::reset(yyjson_doc* doc) noexcept {
  if (doc_) {
    yyjson_doc_free(doc_);
  }
  doc_ = doc;
}

// ============================================================================
// YyJsonMutDoc Implementation
// ============================================================================

YyJsonMutDoc::~YyJsonMutDoc() noexcept {
  if (doc_) {
    yyjson_mut_doc_free(doc_);
  }
}

void YyJsonMutDoc::reset(yyjson_mut_doc* doc) noexcept {
  if (doc_) {
    yyjson_mut_doc_free(doc_);
  }
  doc_ = doc;
}

// ============================================================================
// JsonDocument Implementation
// ============================================================================

JsonDocument::JsonDocument() : doc_(nullptr) {}

JsonDocument::JsonDocument(yyjson_doc* doc) : doc_(doc) {}

JsonDocument::~JsonDocument() {
  if (doc_) {
    yyjson_doc_free(doc_);
  }
}

JsonDocument::JsonDocument(JsonDocument&& other) noexcept : doc_(other.doc_) {
  other.doc_ = nullptr;
}

JsonDocument& JsonDocument::operator=(JsonDocument&& other) noexcept {
  if (this != &other) {
    if (doc_) {
      yyjson_doc_free(doc_);
    }
    doc_ = other.doc_;
    other.doc_ = nullptr;
  }
  return *this;
}

JsonDocument JsonDocument::parse(kj::StringPtr str) {
  yyjson_read_err err;
  yyjson_doc* doc = yyjson_read_opts(const_cast<char*>(str.cStr()), static_cast<size_t>(str.size()),
                                     0, nullptr, &err);

  if (!doc) {
    KJ_FAIL_REQUIRE("JSON parse error", err.pos, err.msg ? err.msg : "unknown error");
  }

  return JsonDocument(doc);
}

JsonDocument JsonDocument::parse_file(const kj::Path& path) {
  auto pathStr = path.toString();
  std::ifstream file(pathStr.cStr(), std::ios::binary | std::ios::ate);
  KJ_REQUIRE(file.is_open(), "Failed to open file", pathStr.cStr());

  std::streamsize size = file.tellg();
  KJ_REQUIRE(size >= 0, "Failed to get file size", pathStr.cStr());
  file.seekg(0, std::ios::beg);

  // Allocate one extra byte for NUL terminator (required by kj::StringPtr)
  auto buf = kj::heapArray<char>(static_cast<size_t>(size) + 1);
  file.read(buf.begin(), size);
  KJ_REQUIRE(file.good(), "Failed to read file", pathStr.cStr());

  // Add NUL terminator so kj::StringPtr can safely wrap the buffer
  buf[static_cast<size_t>(size)] = '\0';

  return parse(kj::StringPtr(buf.begin(), static_cast<size_t>(size)));
}

JsonValue JsonDocument::root() const {
  if (!doc_) {
    return JsonValue(nullptr);
  }
  return JsonValue(yyjson_doc_get_root(doc_));
}

JsonValue JsonDocument::operator[](int index) const {
  return operator[](static_cast<size_t>(index));
}

JsonValue JsonDocument::operator[](size_t index) const {
  if (!doc_) {
    return JsonValue(nullptr);
  }
  return JsonValue(yyjson_arr_get(yyjson_doc_get_root(doc_), index));
}

// ============================================================================
// JsonValue Implementation
// ============================================================================

JsonValue::JsonValue(yyjson_val* val) : val_(val) {}

bool JsonValue::is_null() const {
  return val_ && yyjson_is_null(val_);
}

bool JsonValue::is_bool() const {
  return val_ && yyjson_is_bool(val_);
}

bool JsonValue::is_number() const {
  return val_ && yyjson_is_num(val_);
}

bool JsonValue::is_int() const {
  return val_ && yyjson_is_int(val_);
}

bool JsonValue::is_uint() const {
  return val_ && yyjson_is_uint(val_);
}

bool JsonValue::is_real() const {
  return val_ && yyjson_is_real(val_);
}

bool JsonValue::is_string() const {
  return val_ && yyjson_is_str(val_);
}

bool JsonValue::is_array() const {
  return val_ && yyjson_is_arr(val_);
}

bool JsonValue::is_object() const {
  return val_ && yyjson_is_obj(val_);
}

bool JsonValue::get_bool(bool default_val) const {
  if (!is_bool()) {
    return default_val;
  }
  return yyjson_get_bool(val_);
}

int64_t JsonValue::get_int(int64_t default_val) const {
  if (!is_int() && !is_uint() && !is_real()) {
    return default_val;
  }
  if (is_int()) {
    return yyjson_get_int(val_);
  }
  if (is_uint()) {
    return static_cast<int64_t>(yyjson_get_uint(val_));
  }
  return static_cast<int64_t>(yyjson_get_real(val_));
}

uint64_t JsonValue::get_uint(uint64_t default_val) const {
  if (!is_int() && !is_uint() && !is_real()) {
    return default_val;
  }
  if (is_uint()) {
    return yyjson_get_uint(val_);
  }
  if (is_int()) {
    return static_cast<uint64_t>(yyjson_get_int(val_));
  }
  return static_cast<uint64_t>(yyjson_get_real(val_));
}

double JsonValue::get_double(double default_val) const {
  if (!is_number()) {
    return default_val;
  }
  if (is_real()) {
    return yyjson_get_real(val_);
  }
  if (is_int()) {
    return static_cast<double>(yyjson_get_int(val_));
  }
  return static_cast<double>(yyjson_get_uint(val_));
}

kj::String JsonValue::get_string(kj::StringPtr default_val) const {
  if (!is_string()) {
    return kj::str(default_val);
  }
  const char* str = yyjson_get_str(val_);
  size_t len = yyjson_get_len(val_);
  if (str == nullptr) {
    return kj::str(default_val);
  }
  return kj::heapString(str, len);
}

kj::Maybe<kj::StringPtr> JsonValue::get_string_ptr() const {
  if (!is_string()) {
    return kj::none;
  }
  const char* str = yyjson_get_str(val_);
  if (str == nullptr) {
    return kj::none;
  }
  size_t len = yyjson_get_len(val_);
  return kj::StringPtr(str, len);
}

size_t JsonValue::size() const {
  if (is_array()) {
    return yyjson_arr_size(val_);
  }
  if (is_object()) {
    return yyjson_obj_size(val_);
  }
  return 0;
}

JsonValue JsonValue::operator[](size_t index) const {
  if (!is_array()) {
    return JsonValue(nullptr);
  }
  return JsonValue(yyjson_arr_get(val_, index));
}

JsonValue JsonValue::operator[](kj::StringPtr key) const {
  if (!is_object()) {
    return JsonValue(nullptr);
  }
  return JsonValue(yyjson_obj_getn(val_, key.cStr(), static_cast<size_t>(key.size())));
}

JsonValue JsonValue::operator[](const char* key) const {
  if (!is_object()) {
    return JsonValue(nullptr);
  }
  return JsonValue(yyjson_obj_get(val_, key));
}

kj::Maybe<JsonValue> JsonValue::get(kj::StringPtr key) const {
  if (!is_object()) {
    return kj::none;
  }
  yyjson_val* child = yyjson_obj_getn(val_, key.cStr(), static_cast<size_t>(key.size()));
  if (!child) {
    return kj::none;
  }
  return JsonValue(child);
}

void JsonValue::for_each_array(kj::Function<void(const JsonValue&)> callback) const {
  if (!is_array()) {
    return;
  }
  size_t size = yyjson_arr_size(val_);
  for (size_t i = 0; i < size; i++) {
    yyjson_val* item = yyjson_arr_get(val_, i);
    callback(JsonValue(item));
  }
}

void JsonValue::for_each_object(
    kj::Function<void(kj::StringPtr, const JsonValue&)> callback) const {
  if (!is_object()) {
    return;
  }
  yyjson_obj_iter iter;
  yyjson_obj_iter_init(val_, &iter);
  yyjson_val* key;
  while ((key = yyjson_obj_iter_next(&iter))) {
    yyjson_val* val = yyjson_obj_iter_get_val(key);
    const char* k = yyjson_get_str(key);
    size_t len = yyjson_get_len(key);
    callback(kj::StringPtr(k, len), JsonValue(val));
  }
}

kj::Vector<kj::String> JsonValue::keys() const {
  kj::Vector<kj::String> result;
  if (!is_object()) {
    return result;
  }
  yyjson_obj_iter iter;
  yyjson_obj_iter_init(val_, &iter);
  yyjson_val* key;
  while ((key = yyjson_obj_iter_next(&iter))) {
    const char* k = yyjson_get_str(key);
    size_t len = yyjson_get_len(key);
    result.add(kj::heapString(k, len));
  }
  return result;
}

// ============================================================================
// JsonBuilder Implementation
// ============================================================================

struct JsonBuilder::Impl {
  YyJsonMutDoc doc;         // RAII wrapper for yyjson_mut_doc*
  YyJsonMutValView current; // Non-owning view of current value
  Type type;
  bool is_object; // true for object, false for array

  // Default destructor is sufficient - YyJsonMutDoc handles cleanup
  ~Impl() = default;
};

JsonBuilder::JsonBuilder(Type type) : impl_(kj::heap<Impl>()) {
  impl_->type = type;
  impl_->is_object = (type == Type::Object);
  impl_->doc.reset(yyjson_mut_doc_new(nullptr));
  impl_->current = YyJsonMutValView(impl_->is_object ? yyjson_mut_obj(impl_->doc.get())
                                                     : yyjson_mut_arr(impl_->doc.get()));
}

JsonBuilder::~JsonBuilder() = default;

JsonBuilder::JsonBuilder(JsonBuilder&& other) noexcept : impl_(kj::mv(other.impl_)) {}

JsonBuilder& JsonBuilder::operator=(JsonBuilder&& other) noexcept {
  if (this != &other) {
    impl_ = kj::mv(other.impl_);
  }
  return *this;
}

JsonBuilder JsonBuilder::object() {
  return JsonBuilder(Type::Object);
}

JsonBuilder JsonBuilder::array() {
  return JsonBuilder(Type::Array);
}

JsonBuilder& JsonBuilder::put(kj::StringPtr key, const char* value) {
  if (impl_->is_object) {
    yyjson_mut_val* key_val =
        yyjson_mut_strncpy(impl_->doc.get(), key.cStr(), static_cast<size_t>(key.size()));
    yyjson_mut_val* val_val = yyjson_mut_strcpy(impl_->doc.get(), value);
    if (key_val && val_val) {
      yyjson_mut_obj_add(impl_->current.get(), key_val, val_val);
    }
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(kj::StringPtr key, kj::StringPtr value) {
  if (impl_->is_object) {
    yyjson_mut_val* key_val =
        yyjson_mut_strncpy(impl_->doc.get(), key.cStr(), static_cast<size_t>(key.size()));
    yyjson_mut_val* val_val =
        yyjson_mut_strncpy(impl_->doc.get(), value.cStr(), static_cast<size_t>(value.size()));
    if (key_val && val_val) {
      yyjson_mut_obj_add(impl_->current.get(), key_val, val_val);
    }
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(kj::StringPtr key, bool value) {
  if (impl_->is_object) {
    yyjson_mut_val* key_val =
        yyjson_mut_strncpy(impl_->doc.get(), key.cStr(), static_cast<size_t>(key.size()));
    yyjson_mut_val* val_val = yyjson_mut_bool(impl_->doc.get(), value);
    if (key_val && val_val) {
      yyjson_mut_obj_add(impl_->current.get(), key_val, val_val);
    }
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(kj::StringPtr key, int value) {
  if (impl_->is_object) {
    yyjson_mut_val* key_val =
        yyjson_mut_strncpy(impl_->doc.get(), key.cStr(), static_cast<size_t>(key.size()));
    yyjson_mut_val* val_val = yyjson_mut_int(impl_->doc.get(), value);
    if (key_val && val_val) {
      yyjson_mut_obj_add(impl_->current.get(), key_val, val_val);
    }
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(kj::StringPtr key, int64_t value) {
  if (impl_->is_object) {
    yyjson_mut_val* key_val =
        yyjson_mut_strncpy(impl_->doc.get(), key.cStr(), static_cast<size_t>(key.size()));
    yyjson_mut_val* val_val = yyjson_mut_sint(impl_->doc.get(), value);
    if (key_val && val_val) {
      yyjson_mut_obj_add(impl_->current.get(), key_val, val_val);
    }
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(kj::StringPtr key, uint64_t value) {
  if (impl_->is_object) {
    yyjson_mut_val* key_val =
        yyjson_mut_strncpy(impl_->doc.get(), key.cStr(), static_cast<size_t>(key.size()));
    yyjson_mut_val* val_val = yyjson_mut_uint(impl_->doc.get(), value);
    if (key_val && val_val) {
      yyjson_mut_obj_add(impl_->current.get(), key_val, val_val);
    }
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(kj::StringPtr key, double value) {
  if (impl_->is_object) {
    yyjson_mut_val* key_val =
        yyjson_mut_strncpy(impl_->doc.get(), key.cStr(), static_cast<size_t>(key.size()));
    yyjson_mut_val* val_val = yyjson_mut_real(impl_->doc.get(), value);
    if (key_val && val_val) {
      yyjson_mut_obj_add(impl_->current.get(), key_val, val_val);
    }
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(kj::StringPtr key, std::nullptr_t) {
  if (impl_->is_object) {
    yyjson_mut_val* key_val =
        yyjson_mut_strncpy(impl_->doc.get(), key.cStr(), static_cast<size_t>(key.size()));
    yyjson_mut_val* val_val = yyjson_mut_null(impl_->doc.get());
    if (key_val && val_val) {
      yyjson_mut_obj_add(impl_->current.get(), key_val, val_val);
    }
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(kj::StringPtr key, kj::ArrayPtr<const int> value) {
  if (impl_->is_object) {
    yyjson_mut_val* key_val =
        yyjson_mut_strncpy(impl_->doc.get(), key.cStr(), static_cast<size_t>(key.size()));
    yyjson_mut_val* arr = yyjson_mut_arr(impl_->doc.get());
    for (auto v : value) {
      yyjson_mut_arr_add_int(impl_->doc.get(), arr, v);
    }
    if (key_val && arr) {
      yyjson_mut_obj_add(impl_->current.get(), key_val, arr);
    }
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(kj::StringPtr key, kj::ArrayPtr<const kj::StringPtr> value) {
  if (impl_->is_object) {
    yyjson_mut_val* key_val =
        yyjson_mut_strncpy(impl_->doc.get(), key.cStr(), static_cast<size_t>(key.size()));
    yyjson_mut_val* arr = yyjson_mut_arr(impl_->doc.get());
    for (auto v : value) {
      yyjson_mut_arr_add_strncpy(impl_->doc.get(), arr, v.cStr(), static_cast<size_t>(v.size()));
    }
    if (key_val && arr) {
      yyjson_mut_obj_add(impl_->current.get(), key_val, arr);
    }
  }
  return *this;
}

JsonBuilder& JsonBuilder::put_object(kj::StringPtr key, kj::Function<void(JsonBuilder&)> builder) {
  if (impl_->is_object) {
    yyjson_mut_val* key_val =
        yyjson_mut_strncpy(impl_->doc.get(), key.cStr(), static_cast<size_t>(key.size()));
    yyjson_mut_val* nested = yyjson_mut_obj(impl_->doc.get());
    // Save current state
    YyJsonMutValView saved_current = impl_->current;
    bool saved_is_object = impl_->is_object;
    // Temporarily switch to nested object
    impl_->current = YyJsonMutValView(nested);
    impl_->is_object = true;
    builder(*this);
    // Restore state and add nested object to parent
    impl_->current = saved_current;
    impl_->is_object = saved_is_object;
    if (key_val && nested) {
      yyjson_mut_obj_add(impl_->current.get(), key_val, nested);
    }
  }
  return *this;
}

JsonBuilder& JsonBuilder::put_array(kj::StringPtr key, kj::Function<void(JsonBuilder&)> builder) {
  if (impl_->is_object) {
    yyjson_mut_val* key_val =
        yyjson_mut_strncpy(impl_->doc.get(), key.cStr(), static_cast<size_t>(key.size()));
    yyjson_mut_val* nested = yyjson_mut_arr(impl_->doc.get());
    // Save current state
    YyJsonMutValView saved_current = impl_->current;
    bool saved_is_object = impl_->is_object;
    // Temporarily switch to nested array
    impl_->current = YyJsonMutValView(nested);
    impl_->is_object = false;
    builder(*this);
    // Restore state and add nested array to parent
    impl_->current = saved_current;
    impl_->is_object = saved_is_object;
    if (key_val && nested) {
      yyjson_mut_obj_add(impl_->current.get(), key_val, nested);
    }
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(const char* value) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_strcpy(impl_->doc.get(), impl_->current.get(), value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(kj::StringPtr value) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_strncpy(impl_->doc.get(), impl_->current.get(), value.cStr(),
                               static_cast<size_t>(value.size()));
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(bool value) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_bool(impl_->doc.get(), impl_->current.get(), value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(int value) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_int(impl_->doc.get(), impl_->current.get(), value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(int64_t value) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_sint(impl_->doc.get(), impl_->current.get(), value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(uint64_t value) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_uint(impl_->doc.get(), impl_->current.get(), value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(double value) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_real(impl_->doc.get(), impl_->current.get(), value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(std::nullptr_t) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_null(impl_->doc.get(), impl_->current.get());
  }
  return *this;
}

JsonBuilder& JsonBuilder::add_object(kj::Function<void(JsonBuilder&)> builder) {
  if (!impl_->is_object) {
    yyjson_mut_val* nested = yyjson_mut_obj(impl_->doc.get());
    // Save current state
    YyJsonMutValView saved_current = impl_->current;
    bool saved_is_object = impl_->is_object;
    // Temporarily switch to nested object
    impl_->current = YyJsonMutValView(nested);
    impl_->is_object = true;
    builder(*this);
    // Restore state and add nested object to parent array
    impl_->current = saved_current;
    impl_->is_object = saved_is_object;
    yyjson_mut_arr_add_val(impl_->current.get(), nested);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add_array(kj::Function<void(JsonBuilder&)> builder) {
  if (!impl_->is_object) {
    yyjson_mut_val* nested = yyjson_mut_arr(impl_->doc.get());
    // Save current state
    YyJsonMutValView saved_current = impl_->current;
    bool saved_is_object = impl_->is_object;
    // Temporarily switch to nested array
    impl_->current = YyJsonMutValView(nested);
    impl_->is_object = false;
    builder(*this);
    // Restore state and add nested array to parent array
    impl_->current = saved_current;
    impl_->is_object = saved_is_object;
    yyjson_mut_arr_add_val(impl_->current.get(), nested);
  }
  return *this;
}

kj::String JsonBuilder::build(bool pretty) const {
  if (!impl_->doc) {
    return kj::str(""_kj);
  }
  if (!impl_->current) {
    return kj::str(""_kj);
  }
  // Set the current value as the document root before writing
  yyjson_mut_doc_set_root(impl_->doc.get(), impl_->current.get());
  size_t len = 0;
  yyjson_write_flag flags = pretty ? YYJSON_WRITE_PRETTY : 0;
  yyjson_write_err err;
  char* json = yyjson_mut_write_opts(impl_->doc.get(), flags, nullptr, &len, &err);
  if (json == nullptr) {
    return kj::str(""_kj);
  }
  kj::String result = kj::heapString(json, len);
  free(json);
  return result;
}

// ============================================================================
// json_utils Implementation
// ============================================================================

namespace json_utils {

kj::String escape_string(kj::StringPtr str) {
  JsonBuilder builder = JsonBuilder::array();
  builder.add(str);
  auto json = builder.build();
  kj::StringPtr jsonPtr = json;
  if (jsonPtr.size() >= 3 && jsonPtr[0] == '[' && jsonPtr[jsonPtr.size() - 1] == ']') {
    auto inner = jsonPtr.slice(2, jsonPtr.size() - 2);
    return kj::heapString(inner.begin(), inner.size());
  }
  return kj::heapString(jsonPtr.begin(), jsonPtr.size());
}

kj::String unescape_string(kj::StringPtr str) {
  auto doc = JsonDocument::parse(str);
  return doc.root().get_string();
}

bool is_valid_json(kj::StringPtr str) {
  kj::Maybe<kj::Exception> maybeException =
      kj::runCatchingExceptions([&]() { JsonDocument::parse(str); });
  return maybeException == kj::none;
}

} // namespace json_utils

} // namespace core
} // namespace veloz
