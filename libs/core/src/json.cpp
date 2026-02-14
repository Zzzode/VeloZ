#include "veloz/core/json.h"

#include "veloz/core/logger.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <kj/common.h>
#include <kj/memory.h>
#include <sstream>
#include <stdexcept>
#include <string>

// Include yyjson C header
#include <yyjson.h>

namespace veloz {
namespace core {

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

JsonDocument JsonDocument::parse(const std::string& str) {
  yyjson_read_err err;
  yyjson_doc* doc = yyjson_read_opts(const_cast<char*>(str.data()), str.size(), 0, nullptr, &err);

  if (!doc) {
    throw std::runtime_error(std::format("JSON parse error at position {}: {}", err.pos,
                                         err.msg ? err.msg : "unknown error"));
  }

  return JsonDocument(doc);
}

JsonDocument JsonDocument::parse_file(const std::string& path) {
  // Read file content
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    throw std::runtime_error(std::format("Failed to open file: {}", path));
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::string content(size, '\0');
  if (!file.read(content.data(), size)) {
    throw std::runtime_error(std::format("Failed to read file: {}", path));
  }

  return parse(content);
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

std::string JsonValue::get_string(const std::string& default_val) const {
  if (!is_string()) {
    return default_val;
  }
  const char* str = yyjson_get_str(val_);
  return str ? str : default_val;
}

std::string_view JsonValue::get_string_view(const std::string_view& default_val) const {
  if (!is_string()) {
    return default_val;
  }
  const char* str = yyjson_get_str(val_);
  size_t len = yyjson_get_len(val_);
  return str ? std::string_view(str, len) : default_val;
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

JsonValue JsonValue::operator[](const std::string& key) const {
  if (!is_object()) {
    return JsonValue(nullptr);
  }
  return JsonValue(yyjson_obj_get(val_, key.c_str()));
}

JsonValue JsonValue::operator[](const char* key) const {
  if (!is_object()) {
    return JsonValue(nullptr);
  }
  return JsonValue(yyjson_obj_get(val_, key));
}

std::optional<JsonValue> JsonValue::get(const std::string& key) const {
  if (!is_object()) {
    return std::nullopt;
  }
  yyjson_val* child = yyjson_obj_get(val_, key.c_str());
  if (!child) {
    return std::nullopt;
  }
  return JsonValue(child);
}

void JsonValue::for_each_array(std::function<void(const JsonValue&)> callback) const {
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
    std::function<void(const std::string&, const JsonValue&)> callback) const {
  if (!is_object()) {
    return;
  }
  yyjson_obj_iter iter;
  yyjson_obj_iter_init(val_, &iter);
  yyjson_val* key;
  while ((key = yyjson_obj_iter_next(&iter))) {
    yyjson_val* val = yyjson_obj_iter_get_val(key);
    std::string k = JsonValue(key).get_string();
    callback(k, JsonValue(val));
  }
}

std::vector<std::string> JsonValue::keys() const {
  std::vector<std::string> result;
  if (!is_object()) {
    return result;
  }
  yyjson_obj_iter iter;
  yyjson_obj_iter_init(val_, &iter);
  yyjson_val* key;
  while ((key = yyjson_obj_iter_next(&iter))) {
    result.push_back(JsonValue(key).get_string());
  }
  return result;
}

// ============================================================================
// JsonBuilder Implementation
// ============================================================================

struct JsonBuilder::Impl {
  yyjson_mut_doc* doc;
  yyjson_mut_val* current;
  Type type;
  bool is_object; // true for object, false for array
  bool owns_doc;

  ~Impl() {
    if (owns_doc && doc) {
      yyjson_mut_doc_free(doc);
    }
  }
};

JsonBuilder::JsonBuilder(Type type) : impl_(std::make_unique<Impl>()) {
  impl_->type = type;
  impl_->is_object = (type == Type::Object);
  impl_->owns_doc = true;
  impl_->doc = yyjson_mut_doc_new(nullptr);
  impl_->current = impl_->is_object ? yyjson_mut_obj(impl_->doc) : yyjson_mut_arr(impl_->doc);
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

JsonBuilder& JsonBuilder::put(const std::string& key, const char* value) {
  if (impl_->is_object) {
    yyjson_mut_obj_add_strcpy(impl_->doc, impl_->current, key.c_str(), value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(const std::string& key, const std::string& value) {
  return put(key, value.c_str());
}

JsonBuilder& JsonBuilder::put(const std::string& key, std::string_view value) {
  if (impl_->is_object) {
    yyjson_mut_obj_add_strncpy(impl_->doc, impl_->current, key.c_str(), value.data(), value.size());
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(const std::string& key, bool value) {
  if (impl_->is_object) {
    yyjson_mut_obj_add_bool(impl_->doc, impl_->current, key.c_str(), value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(const std::string& key, int value) {
  if (impl_->is_object) {
    yyjson_mut_obj_add_int(impl_->doc, impl_->current, key.c_str(), value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(const std::string& key, int64_t value) {
  if (impl_->is_object) {
    yyjson_mut_obj_add_sint(impl_->doc, impl_->current, key.c_str(), value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(const std::string& key, uint64_t value) {
  if (impl_->is_object) {
    yyjson_mut_obj_add_uint(impl_->doc, impl_->current, key.c_str(), value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(const std::string& key, double value) {
  if (impl_->is_object) {
    yyjson_mut_obj_add_real(impl_->doc, impl_->current, key.c_str(), value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(const std::string& key, std::nullptr_t) {
  if (impl_->is_object) {
    yyjson_mut_obj_add_null(impl_->doc, impl_->current, key.c_str());
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(const std::string& key, const std::vector<int>& value) {
  if (impl_->is_object) {
    yyjson_mut_val* arr = yyjson_mut_arr(impl_->doc);
    for (int v : value) {
      yyjson_mut_arr_add_int(impl_->doc, arr, v);
    }
    yyjson_mut_obj_add_val(impl_->doc, impl_->current, key.c_str(), arr);
  }
  return *this;
}

JsonBuilder& JsonBuilder::put(const std::string& key, const std::vector<std::string>& value) {
  if (impl_->is_object) {
    yyjson_mut_val* arr = yyjson_mut_arr(impl_->doc);
    for (const std::string& v : value) {
      yyjson_mut_arr_add_strcpy(impl_->doc, arr, v.c_str());
    }
    yyjson_mut_obj_add_val(impl_->doc, impl_->current, key.c_str(), arr);
  }
  return *this;
}

JsonBuilder& JsonBuilder::put_object(const std::string& key,
                                     std::function<void(JsonBuilder&)> builder) {
  if (impl_->is_object) {
    yyjson_mut_val* nested = yyjson_mut_obj(impl_->doc);
    // Save current state
    yyjson_mut_val* saved_current = impl_->current;
    bool saved_is_object = impl_->is_object;
    // Temporarily switch to nested object
    impl_->current = nested;
    impl_->is_object = true;
    builder(*this);
    // Restore state and add nested object to parent
    impl_->current = saved_current;
    impl_->is_object = saved_is_object;
    yyjson_mut_obj_add_val(impl_->doc, impl_->current, key.c_str(), nested);
  }
  return *this;
}

JsonBuilder& JsonBuilder::put_array(const std::string& key,
                                    std::function<void(JsonBuilder&)> builder) {
  if (impl_->is_object) {
    yyjson_mut_val* nested = yyjson_mut_arr(impl_->doc);
    // Save current state
    yyjson_mut_val* saved_current = impl_->current;
    bool saved_is_object = impl_->is_object;
    // Temporarily switch to nested array
    impl_->current = nested;
    impl_->is_object = false;
    builder(*this);
    // Restore state and add nested array to parent
    impl_->current = saved_current;
    impl_->is_object = saved_is_object;
    yyjson_mut_obj_add_val(impl_->doc, impl_->current, key.c_str(), nested);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(const char* value) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_strcpy(impl_->doc, impl_->current, value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(const std::string& value) {
  return add(value.c_str());
}

JsonBuilder& JsonBuilder::add(std::string_view value) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_strncpy(impl_->doc, impl_->current, value.data(), value.size());
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(bool value) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_bool(impl_->doc, impl_->current, value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(int value) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_int(impl_->doc, impl_->current, value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(int64_t value) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_sint(impl_->doc, impl_->current, value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(uint64_t value) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_uint(impl_->doc, impl_->current, value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(double value) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_real(impl_->doc, impl_->current, value);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add(std::nullptr_t) {
  if (!impl_->is_object) {
    yyjson_mut_arr_add_null(impl_->doc, impl_->current);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add_object(std::function<void(JsonBuilder&)> builder) {
  if (!impl_->is_object) {
    yyjson_mut_val* nested = yyjson_mut_obj(impl_->doc);
    // Save current state
    yyjson_mut_val* saved_current = impl_->current;
    bool saved_is_object = impl_->is_object;
    // Temporarily switch to nested object
    impl_->current = nested;
    impl_->is_object = true;
    builder(*this);
    // Restore state and add nested object to parent array
    impl_->current = saved_current;
    impl_->is_object = saved_is_object;
    yyjson_mut_arr_add_val(impl_->current, nested);
  }
  return *this;
}

JsonBuilder& JsonBuilder::add_array(std::function<void(JsonBuilder&)> builder) {
  if (!impl_->is_object) {
    yyjson_mut_val* nested = yyjson_mut_arr(impl_->doc);
    // Save current state
    yyjson_mut_val* saved_current = impl_->current;
    bool saved_is_object = impl_->is_object;
    // Temporarily switch to nested array
    impl_->current = nested;
    impl_->is_object = false;
    builder(*this);
    // Restore state and add nested array to parent array
    impl_->current = saved_current;
    impl_->is_object = saved_is_object;
    yyjson_mut_arr_add_val(impl_->current, nested);
  }
  return *this;
}

std::string JsonBuilder::build(bool pretty) const {
  if (impl_->doc == nullptr) {
    return "";
  }
  if (impl_->current == nullptr) {
    return "";
  }
  // Set the current value as the document root before writing
  yyjson_mut_doc_set_root(impl_->doc, impl_->current);
  size_t len = 0;
  yyjson_write_flag flags = pretty ? YYJSON_WRITE_PRETTY : 0;
  yyjson_write_err err;
  char* json = yyjson_mut_write_opts(impl_->doc, flags, nullptr, &len, &err);
  if (json == nullptr) {
    return "";
  }
  std::string result(json, len);
  free(json); // free memory allocated by yyjson_mut_write_opts
  return result;
}

// ============================================================================
// json_utils Implementation
// ============================================================================

namespace json_utils {

std::string escape_string(std::string_view str) {
  JsonBuilder builder = JsonBuilder::array();
  builder.add(str);
  std::string json = builder.build();
  // Remove surrounding brackets and quotes
  if (json.size() >= 3 && json.front() == '[' && json.back() == ']') {
    return json.substr(2, json.size() - 4); // remove [" and "]
  }
  return json;
}

std::string unescape_string(std::string_view str) {
  auto doc = JsonDocument::parse(std::string(str));
  return doc.root().get_string();
}

bool is_valid_json(std::string_view str) {
  try {
    JsonDocument::parse(std::string(str));
    return true;
  } catch (...) {
    return false;
  }
}

} // namespace json_utils

} // namespace core
} // namespace veloz
