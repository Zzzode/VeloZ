/**
 * @file json.h
 * @brief High-performance JSON wrapper using yyjson
 *
 * This file provides a C++ RAII wrapper around yyjson for fast JSON parsing
 * and serialization. yyjson is MIT licensed and provides excellent performance.
 *
 * Usage:
 *   auto doc = JsonDocument::parse(json_string);
 *   auto root = doc.root();
 *   double price = root.get("price").value().get_double();
 *
 *   auto builder = JsonBuilder::object();
 *   builder.put("symbol", "BTCUSDT").put("price", 50000.5);
 *   std::string json = builder.build();
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Forward declarations for yyjson types to avoid including C header
struct yyjson_doc;
struct yyjson_val;
struct yyjson_mut_doc;
struct yyjson_mut_val;

namespace veloz::core {

// Forward declaration
class JsonValue;

/**
 * @brief RAII wrapper for yyjson document
 *
 * Manages the lifetime of yyjson document and provides access to root value.
 */
class JsonDocument {
public:
  JsonDocument();
  ~JsonDocument();

  // Non-copyable, movable
  JsonDocument(const JsonDocument&) = delete;
  JsonDocument& operator=(const JsonDocument&) = delete;
  JsonDocument(JsonDocument&& other) noexcept;
  JsonDocument& operator=(JsonDocument&& other) noexcept;

  /**
   * @brief Parse JSON from string
   * @param str JSON string to parse
   * @return JsonDocument with parsed content
   * @throws std::runtime_error if parsing fails
   */
  static JsonDocument parse(const std::string& str);

  /**
   * @brief Parse JSON from file
   * @param path Path to JSON file
   * @return JsonDocument with parsed content
   * @throws std::runtime_error if file read or parsing fails
   */
  static JsonDocument parse_file(const std::string& path);

  /**
   * @brief Get root JSON value
   * @return Root value of the document
   */
  JsonValue root() const;

  /**
   * @brief Check if document is valid
   */
  bool is_valid() const {
    return doc_ != nullptr;
  }

  /**
   * @brief Parse root as specific type (type-safe shortcut)
   *
   * This is the recommended way to parse JSON values as it provides
   * compile-time type checking and avoids string conversion.
   *
   * Usage: auto price = JsonDocument::parse(json_str).parse_as<double>();
   *
   * @tparam T The type to parse as (bool, int, int32_t, int64_t, uint32_t, uint64_t, float, double,
   * std::string)
   * @return The parsed value or std::nullopt if type mismatch
   */
  template <typename T> std::optional<T> parse_as() const;

  /**
   * @brief Parse root as specific type with default value
   *
   * @tparam T The type to parse as
   * @param default_val Default value to return on type mismatch
   * @return The parsed value or default_val
   */
  template <typename T> T parse_as_or(T default_val) const;

  JsonValue operator[](int index) const;
  JsonValue operator[](size_t index) const;

private:
  explicit JsonDocument(yyjson_doc* doc);
  yyjson_doc* doc_;
  friend class JsonValue;
};

/**
 * @brief Read-only view of a JSON value
 *
 * Provides type-safe access to JSON values with optional support for
 * safe navigation through nested structures.
 */
class JsonValue {
public:
  explicit JsonValue(yyjson_val* val = nullptr);

  /**
   * @brief Check if value is null
   */
  bool is_null() const;
  bool is_bool() const;
  bool is_number() const;
  bool is_int() const;
  bool is_uint() const;
  bool is_real() const;
  bool is_string() const;
  bool is_array() const;
  bool is_object() const;

  /**
   * @brief Get boolean value
   * @param default_val Value to return if not a boolean
   */
  bool get_bool(bool default_val = false) const;

  /**
   * @brief Get integer value
   * @param default_val Value to return if not an integer
   */
  int64_t get_int(int64_t default_val = 0) const;

  /**
   * @brief Get unsigned integer value
   * @param default_val Value to return if not an unsigned integer
   */
  uint64_t get_uint(uint64_t default_val = 0) const;

  /**
   * @brief Get floating-point value
   * @param default_val Value to return if not a number
   */
  double get_double(double default_val = 0.0) const;

  /**
   * @brief Get string value
   * @param default_val Value to return if not a string
   */
  std::string get_string(const std::string& default_val = "") const;

  /**
   * @brief Get string as string_view for zero-copy access
   * @param default_val Value to return if not a string
   */
  std::string_view get_string_view(const std::string_view& default_val = "") const;

  /**
   * @brief Get array size
   * @return Number of elements in array, 0 if not an array
   */
  size_t size() const;

  /**
   * @brief Access array element by index
   * @param index Array index
   * @return JsonValue at index, or invalid JsonValue if out of bounds
   */
  JsonValue operator[](int index) const {
    return operator[](static_cast<size_t>(index));
  }
  JsonValue operator[](size_t index) const;

  /**
   * @brief Access object property by key
   * @param key Property key
   * @return JsonValue for the property, or invalid JsonValue if not found
   */
  JsonValue operator[](const std::string& key) const;
  JsonValue operator[](const char* key) const;

  /**
   * @brief Get object property by key
   * @param key Property key
   * @return JsonValue wrapped in optional, or nullopt if not found
   */
  std::optional<JsonValue> get(const std::string& key) const;

  /**
   * @brief Iterate over array elements
   * @param callback Function to call for each element
   */
  void for_each_array(std::function<void(const JsonValue&)> callback) const;

  /**
   * @brief Iterate over object key-value pairs
   * @param callback Function to call for each pair
   */
  void for_each_object(std::function<void(const std::string&, const JsonValue&)> callback) const;

  /**
   * @brief Get all keys from object
   * @return Vector of keys, empty if not an object
   */
  std::vector<std::string> keys() const;

  /**
   * @brief Check if value is valid
   */
  bool is_valid() const {
    return val_ != nullptr;
  }

  /**
   * @brief Get raw yyjson value pointer (for advanced usage)
   */
  yyjson_val* raw() const {
    return val_;
  }

  /**
   * @brief Type-safe parsing as specific type
   *
   * This is the recommended way to parse JSON values as it provides
   * compile-time type checking and avoids string conversion.
   *
   * Usage: double price = value.parse_as<double>();
   *
   * @tparam T The type to parse as (bool, int, int32_t, int64_t, uint32_t, uint64_t, float, double,
   * std::string)
   * @return The parsed value or std::nullopt if type mismatch
   */
  template <typename T> std::optional<T> parse_as() const {
    using DecayT = std::decay_t<T>;

    if constexpr (std::is_same_v<DecayT, bool>) {
      if (!is_bool())
        return std::nullopt;
      return get_bool();
    } else if constexpr (std::is_same_v<DecayT, int> || std::is_same_v<DecayT, int32_t>) {
      if (!is_int() && !is_uint())
        return std::nullopt;
      return static_cast<int>(get_int());
    } else if constexpr (std::is_same_v<DecayT, int64_t>) {
      if (!is_int() && !is_uint())
        return std::nullopt;
      return get_int();
    } else if constexpr (std::is_same_v<DecayT, uint32_t> || std::is_same_v<DecayT, unsigned int>) {
      if (!is_uint())
        return std::nullopt;
      return static_cast<uint32_t>(get_uint());
    } else if constexpr (std::is_same_v<DecayT, uint64_t>) {
      if (!is_uint())
        return std::nullopt;
      return get_uint();
    } else if constexpr (std::is_same_v<DecayT, float> || std::is_same_v<DecayT, double>) {
      if (!is_real() && !is_int() && !is_uint())
        return std::nullopt;
      return static_cast<DecayT>(get_double());
    } else if constexpr (std::is_same_v<DecayT, std::string>) {
      if (!is_string())
        return std::nullopt;
      return get_string();
    } else {
      static_assert(always_false_v<T>, "Unsupported type for parse_as<T>");
      return std::nullopt;
    }
  }

  /**
   * @brief Parse as specific type with default value
   *
   * Similar to parse_as<T>() but returns the provided default on failure.
   *
   * @tparam T The type to parse as
   * @param default_val Default value to return on type mismatch
   * @return The parsed value or default_val
   */
  template <typename T> T parse_as_or(T default_val) const {
    return parse_as<T>().value_or(default_val);
  }

  /**
   * @brief Parse array as vector of type T
   *
   * Usage: auto numbers = value.parse_as_vector<double>();
   *
   * @tparam T The element type (bool, int, int32_t, int64_t, uint32_t, uint64_t, float, double,
   * std::string)
   * @return Vector of parsed values or empty vector on failure
   */
  template <typename T> std::vector<T> parse_as_vector() const {
    using DecayT = std::decay_t<T>;
    std::vector<T> result;

    if (!is_array()) {
      return result;
    }

    if constexpr (std::is_same_v<DecayT, bool>) {
      for_each_array([&result](const JsonValue& val) {
        if (val.is_bool()) {
          result.push_back(val.get_bool());
        }
      });
    } else if constexpr (std::is_same_v<DecayT, int> || std::is_same_v<DecayT, int32_t>) {
      for_each_array([&result](const JsonValue& val) {
        if (val.is_int() || val.is_uint()) {
          result.push_back(static_cast<int>(val.get_int()));
        }
      });
    } else if constexpr (std::is_same_v<DecayT, int64_t>) {
      for_each_array([&result](const JsonValue& val) {
        if (val.is_int() || val.is_uint()) {
          result.push_back(val.get_int());
        }
      });
    } else if constexpr (std::is_same_v<DecayT, uint32_t> || std::is_same_v<DecayT, unsigned int>) {
      for_each_array([&result](const JsonValue& val) {
        if (val.is_uint()) {
          result.push_back(static_cast<uint32_t>(val.get_uint()));
        } else if (val.is_int()) {
          result.push_back(static_cast<uint32_t>(val.get_int()));
        }
      });
    } else if constexpr (std::is_same_v<DecayT, uint64_t>) {
      for_each_array([&result](const JsonValue& val) {
        if (val.is_uint()) {
          result.push_back(val.get_uint());
        } else if (val.is_int()) {
          result.push_back(static_cast<uint64_t>(val.get_int()));
        }
      });
    } else if constexpr (std::is_same_v<DecayT, float> || std::is_same_v<DecayT, double>) {
      for_each_array([&result](const JsonValue& val) {
        if (val.is_real() || val.is_int() || val.is_uint()) {
          result.push_back(static_cast<DecayT>(val.get_double()));
        }
      });
    } else if constexpr (std::is_same_v<DecayT, std::string>) {
      for_each_array([&result](const JsonValue& val) {
        if (val.is_string()) {
          result.push_back(val.get_string());
        }
      });
    } else {
      static_assert(always_false_v<T>, "Unsupported element type for parse_as_vector<T>");
    }

    return result;
  }

private:
  // Helper for static_assert
  template <typename T> static constexpr bool always_false_v = false;

  yyjson_val* val_;
};

template <typename T> std::optional<T> JsonDocument::parse_as() const {
  return root().parse_as<T>();
}

template <typename T> T JsonDocument::parse_as_or(T default_val) const {
  return root().parse_as_or(default_val);
}

/**
 * @brief Builder for creating JSON documents
 *
 * Provides fluent API for constructing JSON objects and arrays.
 * Uses yyjson mutator API for efficient document building.
 */
class JsonBuilder {
public:
  /**
   * @brief Create an object builder
   */
  static JsonBuilder object();

  /**
   * @brief Create an array builder
   */
  static JsonBuilder array();

  ~JsonBuilder();
  JsonBuilder(const JsonBuilder&) = delete;
  JsonBuilder& operator=(const JsonBuilder&) = delete;
  JsonBuilder(JsonBuilder&& other) noexcept;
  JsonBuilder& operator=(JsonBuilder&& other) noexcept;

  /**
   * @brief Add key-value pair to object
   */
  JsonBuilder& put(const std::string& key, const char* value);
  JsonBuilder& put(const std::string& key, const std::string& value);
  JsonBuilder& put(const std::string& key, std::string_view value);
  JsonBuilder& put(const std::string& key, bool value);
  JsonBuilder& put(const std::string& key, int value);
  JsonBuilder& put(const std::string& key, int64_t value);
  JsonBuilder& put(const std::string& key, uint64_t value);
  JsonBuilder& put(const std::string& key, double value);
  JsonBuilder& put(const std::string& key, std::nullptr_t);
  JsonBuilder& put(const std::string& key, const std::vector<int>& value);
  JsonBuilder& put(const std::string& key, const std::vector<std::string>& value);

  /**
   * @brief Add nested object
   */
  JsonBuilder& put_object(const std::string& key, std::function<void(JsonBuilder&)> builder);

  /**
   * @brief Add nested array
   */
  JsonBuilder& put_array(const std::string& key, std::function<void(JsonBuilder&)> builder);

  /**
   * @brief Add value to array
   */
  JsonBuilder& add(const char* value);
  JsonBuilder& add(const std::string& value);
  JsonBuilder& add(std::string_view value);
  JsonBuilder& add(bool value);
  JsonBuilder& add(int value);
  JsonBuilder& add(int64_t value);
  JsonBuilder& add(uint64_t value);
  JsonBuilder& add(double value);
  JsonBuilder& add(std::nullptr_t);

  /**
   * @brief Add nested object to array
   */
  JsonBuilder& add_object(std::function<void(JsonBuilder&)> builder);

  /**
   * @brief Add nested array to array
   */
  JsonBuilder& add_array(std::function<void(JsonBuilder&)> builder);

  /**
   * @brief Build JSON string
   * @param pretty Pretty print with indentation
   * @return JSON string
   */
  std::string build(bool pretty = false) const;

private:
  enum class Type { Object, Array };
  explicit JsonBuilder(Type type);

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * @brief Utility functions for JSON operations
 */
namespace json_utils {

/**
 * @brief Escape string for JSON
 */
std::string escape_string(std::string_view str);

/**
 * @brief Unescape JSON string
 */
std::string unescape_string(std::string_view str);

/**
 * @brief Validate JSON string without parsing
 * @return true if valid JSON
 */
bool is_valid_json(std::string_view str);

} // namespace json_utils

} // namespace veloz::core
