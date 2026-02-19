/**
 * @file config_manager.h
 * @brief Advanced configuration management utilities for VeloZ
 *
 * This file provides advanced configuration management utilities including:
 * - ConfigItem<T>: Type-safe configuration items with validation
 * - ConfigGroup: Hierarchical organization of config items
 * - ConfigManager: Centralized configuration with hot reload support
 * - JSON/YAML loaders: Configuration file parsing
 *
 * These utilities provide a robust, type-safe way to manage application
 * configuration with validation, hot-reload, and hierarchical organization.
 */

#pragma once

#include "veloz/core/json.h"

#include <atomic>
#include <kj/common.h>
#include <kj/filesystem.h> // kj::Path for file paths
#include <kj/function.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <memory> // std::unique_ptr used for polymorphic ownership (ConfigItemBase)
#include <mutex>  // std::mutex kept for simple synchronization (not guarding specific values)
#include <stdexcept>
#include <string>
#include <variant>
#include <vector> // std::vector kept for ConfigValue and external API compatibility

namespace veloz::core {

/**
 * @brief Forward declaration
 */
class ConfigGroup;
class ConfigManager;

/**
 * @brief Configuration item type
 *
 * Enumerates the supported types for configuration values.
 */
enum class ConfigItemType {
  Bool,
  Int,
  Int64,
  Double,
  String,
  BoolArray,
  IntArray,
  Int64Array,
  DoubleArray,
  StringArray
};

/**
 * @brief Configuration value type
 *
 * Variant type that can hold any supported configuration value.
 * Note: Uses std::variant because KJ library does not provide an equivalent variant type.
 * KJ's approach to variants is using inheritance or Maybe patterns, but for this
 * config system, std::variant provides the best type safety and ergonomics.
 */
using ConfigValue =
    std::variant<bool, int, int64_t, double, std::string, std::vector<bool>, std::vector<int>,
                 std::vector<int64_t>, std::vector<double>, std::vector<std::string>>;

/**
 * @brief Validation function signature
 *
 * Used to validate configuration values before they are set.
 */
template <typename T> using ConfigValidator = kj::Function<bool(const T&)>;

/**
 * @brief Change callback function signature
 *
 * Called when a configuration value changes.
 */
template <typename T>
using ConfigChangeCallback = kj::Function<void(const T& old_value, const T& new_value)>;

/**
 * @brief Base configuration item interface
 *
 * Provides type-erased access to configuration items.
 */
class ConfigItemBase {
public:
  virtual ~ConfigItemBase() = default;

  /**
   * @brief Get key/name of this configuration item
   */
  [[nodiscard]] virtual kj::StringPtr key() const noexcept = 0;

  /**
   * @brief Get description of this configuration item
   */
  [[nodiscard]] virtual kj::StringPtr description() const noexcept = 0;

  /**
   * @brief Get type of this configuration item
   */
  [[nodiscard]] virtual ConfigItemType type() const noexcept = 0;

  /**
   * @brief Check if this configuration item is required
   */
  [[nodiscard]] virtual bool is_required() const noexcept = 0;

  /**
   * @brief Check if this configuration item has been set
   */
  [[nodiscard]] virtual bool is_set() const noexcept = 0;

  /**
   * @brief Check if this configuration item has a default value
   */
  [[nodiscard]] virtual bool has_default() const noexcept = 0;

  /**
   * @brief Reset to default value (if available)
   */
  virtual void reset() = 0;

  /**
   * @brief Get value as a string
   */
  [[nodiscard]] virtual kj::String to_string() const = 0;

  /**
   * @brief Set value from a string
   */
  virtual bool from_string(kj::StringPtr value) = 0;

  /**
   * @brief Convert item value to JSON value string (for serialization)
   */
  [[nodiscard]] virtual kj::String to_json_string() const = 0;
};

/**
 * @brief Type-safe configuration item
 *
 * Represents a single configuration value with type safety,
 * validation, and change notification support.
 *
 * @tparam T The type of configuration value
 */
template <typename T> class ConfigItem : public ConfigItemBase {
public:
  ~ConfigItem() noexcept override = default;

  /**
   * @brief Builder for ConfigItem
   */
  class Builder {
  public:
    Builder(kj::StringPtr key, kj::StringPtr description)
        : key_(kj::str(key)), description_(kj::str(description)) {}

    /**
     * @brief Set default value
     */
    Builder& default_value(T value) {
      default_ = kj::mv(value);
      has_default_ = true;
      return *this;
    }

    /**
     * @brief Set whether this item is required
     */
    Builder& required(bool required = true) {
      required_ = required;
      return *this;
    }

    /**
     * @brief Set a validator function
     */
    Builder& validator(ConfigValidator<T> validator) {
      validator_ = kj::Maybe<ConfigValidator<T>>(kj::mv(validator));
      return *this;
    }

    /**
     * @brief Add a change callback
     */
    Builder& on_change(ConfigChangeCallback<T> callback) {
      on_change_callbacks_.add(kj::mv(callback));
      return *this;
    }

    /**
     * @brief Build ConfigItem
     * Note: Uses std::make_unique because ConfigItemBase requires std::unique_ptr for
     * polymorphic ownership (kj::Own does not support release() for this pattern)
     */
    kj::Own<ConfigItem<T>> build() {
      auto item =
          kj::heap<ConfigItem<T>>(kj::mv(key_), kj::mv(description_), required_, has_default_);

      if (has_default_) {
        item->default_value_ = kj::mv(default_);
        item->value_ = item->default_value_;
      }

      item->validator_ = kj::mv(validator_);
      item->on_change_callbacks_ = kj::mv(on_change_callbacks_);

      return item;
    }

  private:
    kj::String key_;
    kj::String description_;
    T default_{};
    bool has_default_{false};
    bool required_{false};
    kj::Maybe<ConfigValidator<T>> validator_;
    kj::Vector<ConfigChangeCallback<T>> on_change_callbacks_;
  };

  /**
   * @brief Constructor (use Builder instead)
   */
  ConfigItem(kj::String key, kj::String description, bool required, bool has_default)
      : key_(kj::mv(key)), description_(kj::mv(description)), required_(required),
        has_default_(has_default) {
    is_set_.store(has_default);
  }

  // Getters
  [[nodiscard]] kj::StringPtr key() const noexcept override {
    return key_;
  }
  [[nodiscard]] kj::StringPtr description() const noexcept override {
    return description_;
  }
  [[nodiscard]] ConfigItemType type() const noexcept override;
  [[nodiscard]] bool is_required() const noexcept override {
    return required_;
  }
  [[nodiscard]] bool is_set() const noexcept override {
    return is_set_;
  }
  [[nodiscard]] bool has_default() const noexcept override {
    return has_default_;
  }

  /**
   * @brief Get current value
   * @return kj::Maybe value, kj::none if not set
   */
  [[nodiscard]] kj::Maybe<T> get() const {
    std::scoped_lock lock(mu_);
    return is_set_ ? kj::Maybe<T>(value_) : kj::none;
  }

  /**
   * @brief Get value or a default
   * @param default_value Default to return if not set
   */
  [[nodiscard]] T get_or(T default_value) const {
    std::scoped_lock lock(mu_);
    return is_set_ ? value_ : kj::mv(default_value);
  }

  /**
   * @brief Get value or throw if not set
   */
  [[nodiscard]] const T& value() const {
    std::scoped_lock lock(mu_);
    if (!is_set_) {
      KJ_FAIL_REQUIRE("Config item not set and has no default", kj::str(key_, " (value() called)"));
    }
    return value_;
  }

  /**
   * @brief Get default value
   */
  [[nodiscard]] kj::Maybe<T> default_value() const {
    std::scoped_lock lock(mu_);
    return has_default_ ? kj::Maybe<T>(default_value_) : kj::none;
  }

  /**
   * @brief Set value
   * @param value New value to set
   * @return true if successful, false if validation fails
   */
  bool set(const T& value) {
    std::scoped_lock lock(mu_);
    return set_locked(value);
  }

  /**
   * @brief Set value (move version)
   */
  bool set(T&& value) {
    std::scoped_lock lock(mu_);
    return set_locked(kj::mv(value));
  }

  void reset() override {
    std::scoped_lock lock(mu_);
    if (has_default_) {
      value_ = default_value_;
      is_set_ = true;
    } else {
      is_set_ = false;
    }
  }

  [[nodiscard]] kj::String to_string() const override {
    std::scoped_lock lock(mu_);
    if constexpr (std::is_same_v<T, bool>) {
      return is_set_ ? (value_ ? kj::str("true"_kj) : kj::str("false"_kj)) : kj::str("not set"_kj);
    } else if constexpr (std::is_same_v<T, std::string>) {
      // Convert std::string to c_str() for kj::str() compatibility
      return is_set_ ? kj::str("\"", value_.c_str(), "\"") : kj::str("not set"_kj);
    } else if constexpr (std::is_same_v<T, std::vector<bool>> ||
                         std::is_same_v<T, std::vector<int>> ||
                         std::is_same_v<T, std::vector<int64_t>> ||
                         std::is_same_v<T, std::vector<double>> ||
                         std::is_same_v<T, std::vector<std::string>>) {
      if (!is_set_)
        return kj::str("not set"_kj);
      return array_to_string(value_);
    } else {
      // std::to_string returns std::string, convert to c_str() for kj::str()
      return is_set_ ? kj::heapString(std::to_string(value_).c_str()) : kj::str("not set"_kj);
    }
  }

  bool from_string(kj::StringPtr value) override {
    if constexpr (std::is_same_v<T, bool>) {
      if (value == "true"_kj || value == "1"_kj)
        return set(true);
      if (value == "false"_kj || value == "0"_kj)
        return set(false);
      return false;
    } else if constexpr (std::is_same_v<T, int>) {
      try {
        return set(std::stoi(std::string(value.cStr())));
      } catch (...) {
        return false;
      }
    } else if constexpr (std::is_same_v<T, int64_t>) {
      try {
        return set(std::stoll(std::string(value.cStr())));
      } catch (...) {
        return false;
      }
    } else if constexpr (std::is_same_v<T, double>) {
      try {
        return set(std::stod(std::string(value.cStr())));
      } catch (...) {
        return false;
      }
    } else if constexpr (std::is_same_v<T, std::string>) {
      return set(std::string(value.cStr()));
    } else if constexpr (std::is_same_v<T, std::vector<bool>> ||
                         std::is_same_v<T, std::vector<int>> ||
                         std::is_same_v<T, std::vector<int64_t>> ||
                         std::is_same_v<T, std::vector<double>> ||
                         std::is_same_v<T, std::vector<std::string>>) {
      try {
        auto j = veloz::core::JsonDocument::parse(std::string(value.cStr()));
        if (j.root().is_array()) {
          // Parse array type
          if constexpr (std::is_same_v<T, std::vector<bool>>) {
            std::vector<bool> vec;
            j.root().for_each_array(
                [&vec](const veloz::core::JsonValue& val) { vec.push_back(val.get_bool()); });
            return set(vec);
          } else if constexpr (std::is_same_v<T, std::vector<int>>) {
            std::vector<int> vec;
            j.root().for_each_array([&vec](const veloz::core::JsonValue& val) {
              vec.push_back(static_cast<int>(val.get_int()));
            });
            return set(vec);
          } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
            std::vector<int64_t> vec;
            j.root().for_each_array(
                [&vec](const veloz::core::JsonValue& val) { vec.push_back(val.get_int()); });
            return set(vec);
          } else if constexpr (std::is_same_v<T, std::vector<double>>) {
            std::vector<double> vec;
            j.root().for_each_array(
                [&vec](const veloz::core::JsonValue& val) { vec.push_back(val.get_double()); });
            return set(vec);
          } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            std::vector<std::string> vec;
            j.root().for_each_array(
                [&vec](const veloz::core::JsonValue& val) { vec.push_back(val.get_string()); });
            return set(vec);
          }
          return false;
        }
        return false;
      } catch (...) {
        return false;
      }
    }
    return false;
  }

  /**
   * @brief Add a change callback
   */
  void add_callback(ConfigChangeCallback<T> callback) {
    std::scoped_lock lock(mu_);
    on_change_callbacks_.add(kj::mv(callback));
  }

  /**
   * @brief Convert item value to JSON value string (for serialization)
   */
  [[nodiscard]] kj::String to_json_string() const override {
    if (!is_set_) {
      return kj::str("null"_kj);
    }

    std::scoped_lock lock(mu_);
    if constexpr (std::is_same_v<T, std::vector<bool>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (bool item : value_) {
        arr_builder.add(item);
      }
      std::string built = arr_builder.build();
      // Convert std::string to kj::String via c_str() for kj::heapString compatibility
      return kj::heapString(built.c_str());
    } else if constexpr (std::is_same_v<T, std::vector<int>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (int item : value_) {
        arr_builder.add(item);
      }
      std::string built = arr_builder.build();
      return kj::heapString(built.c_str());
    } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (int64_t item : value_) {
        arr_builder.add(item);
      }
      std::string built = arr_builder.build();
      return kj::heapString(built.c_str());
    } else if constexpr (std::is_same_v<T, std::vector<double>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (double item : value_) {
        arr_builder.add(item);
      }
      std::string built = arr_builder.build();
      return kj::heapString(built.c_str());
    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (const std::string& item : value_) {
        arr_builder.add(item);
      }
      std::string built = arr_builder.build();
      return kj::heapString(built.c_str());
    } else {
      veloz::core::JsonBuilder builder = veloz::core::JsonBuilder::object();
      builder.put("value", value_);
      auto doc = veloz::core::JsonDocument::parse(builder.build());
      auto root = doc.root();

      if constexpr (std::is_same_v<T, bool>) {
        return root["value"].get_bool() ? kj::str("true"_kj) : kj::str("false"_kj);
      } else if constexpr (std::is_integral_v<T>) {
        // std::to_string returns std::string, convert to c_str() for kj::str()
        return kj::heapString(std::to_string(root["value"].get_int()).c_str());
      } else if constexpr (std::is_floating_point_v<T>) {
        // std::to_string returns std::string, convert to c_str() for kj::str()
        return kj::heapString(std::to_string(root["value"].get_double()).c_str());
      } else if constexpr (std::is_same_v<T, std::string>) {
        // get_string() returns std::string, convert to c_str() for kj::str()
        std::string str_val = root["value"].get_string();
        return kj::str("\"", str_val.c_str(), "\"");
      }
    }

    return kj::str("null"_kj);
  }

private:
  template <typename U> bool set_locked(U&& value) {
    KJ_IF_SOME(v, validator_) {
      if (!v(value)) {
        return false;
      }
    }

    T old_value = is_set_ ? value_ : T{};
    bool was_set = is_set_;
    value_ = std::forward<U>(value);
    is_set_ = true;

    // Notify callbacks if value changed or was previously unset
    if (!was_set || old_value != value_) {
      for (auto& callback : on_change_callbacks_) {
        callback(old_value, value_);
      }
    }

    return true;
  }

  [[nodiscard]] static kj::String array_to_string(const std::vector<bool>& v) {
    kj::Vector<kj::String> parts;
    parts.reserve(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
      parts.add(kj::str(v[i] ? "true"_kj : "false"_kj));
    }
    if (parts.size() == 0) {
      return kj::str("[]"_kj);
    }
    kj::String result = kj::str("["_kj);
    for (size_t i = 0; i < parts.size(); ++i) {
      if (i > 0) {
        result = kj::str(result, ", "_kj, parts[i]);
      } else {
        result = kj::str(result, parts[i]);
      }
    }
    return kj::str(result, "]"_kj);
  }

  template <typename V> [[nodiscard]] static kj::String array_to_string(const std::vector<V>& v) {
    kj::Vector<kj::String> parts;
    parts.reserve(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
      if constexpr (std::is_same_v<V, std::string>) {
        parts.add(kj::str("\"", v[i].c_str(), "\""));
      } else {
        // std::to_string returns std::string, convert to c_str() for kj::str()
        parts.add(kj::heapString(std::to_string(v[i]).c_str()));
      }
    }
    if (parts.size() == 0) {
      return kj::str("[]"_kj);
    }
    kj::String result = kj::str("["_kj);
    for (size_t i = 0; i < parts.size(); ++i) {
      if (i > 0) {
        result = kj::str(result, ", "_kj, parts[i]);
      } else {
        result = kj::str(result, parts[i]);
      }
    }
    return kj::str(result, "]"_kj);
  }

  kj::String key_;
  kj::String description_;
  T default_value_;
  T value_;
  bool required_;
  bool has_default_;
  std::atomic<bool> is_set_{false};
  kj::Maybe<ConfigValidator<T>> validator_;
  kj::Vector<ConfigChangeCallback<T>> on_change_callbacks_;
  mutable std::mutex mu_; // std::mutex for compatibility with std::scoped_lock
};

/**
 * @brief Type trait to map types to ConfigItemType
 */
template <typename T> struct ConfigTypeTraits;

template <> struct ConfigTypeTraits<bool> {
  static constexpr ConfigItemType type = ConfigItemType::Bool;
};

template <> struct ConfigTypeTraits<int> {
  static constexpr ConfigItemType type = ConfigItemType::Int;
};

template <> struct ConfigTypeTraits<int64_t> {
  static constexpr ConfigItemType type = ConfigItemType::Int64;
};

template <> struct ConfigTypeTraits<double> {
  static constexpr ConfigItemType type = ConfigItemType::Double;
};

template <> struct ConfigTypeTraits<std::string> {
  static constexpr ConfigItemType type = ConfigItemType::String;
};

template <> struct ConfigTypeTraits<std::vector<bool>> {
  static constexpr ConfigItemType type = ConfigItemType::BoolArray;
};

template <> struct ConfigTypeTraits<std::vector<int>> {
  static constexpr ConfigItemType type = ConfigItemType::IntArray;
};

template <> struct ConfigTypeTraits<std::vector<int64_t>> {
  static constexpr ConfigItemType type = ConfigItemType::Int64Array;
};

template <> struct ConfigTypeTraits<std::vector<double>> {
  static constexpr ConfigItemType type = ConfigItemType::DoubleArray;
};

template <> struct ConfigTypeTraits<std::vector<std::string>> {
  static constexpr ConfigItemType type = ConfigItemType::StringArray;
};

template <typename T> ConfigItemType ConfigItem<T>::type() const noexcept {
  return ConfigTypeTraits<T>::type;
}

/**
 * @brief Configuration group
 *
 * Organizes configuration items hierarchically.
 * Groups can contain other groups and config items.
 */
class ConfigGroup final {
public:
  explicit ConfigGroup(kj::StringPtr name, kj::StringPtr description = ""_kj)
      : name_(kj::str(name)), description_(kj::str(description)) {}

  /**
   * @brief Add a config item to this group
   * @param item Config item to add (takes ownership)
   */
  void add_item(kj::Own<ConfigItemBase> item) {
    std::scoped_lock lock(mu_);
    auto key_view = item->key();
    kj::String key_str = kj::heapString(key_view);
    items_.upsert(kj::mv(key_str), kj::mv(item),
                  [](auto& existing, auto&& newVal) { existing = kj::mv(newVal); });
  }

  /**
   * @brief Add a sub-group
   * @param group Group to add (takes ownership)
   */
  void add_group(kj::Own<ConfigGroup> group) {
    std::scoped_lock lock(mu_);
    kj::String name_str = kj::heapString(group->name_);
    groups_.upsert(kj::mv(name_str), kj::mv(group),
                   [](auto& existing, auto&& newVal) { existing = kj::mv(newVal); });
  }

  /**
   * @brief Get a config item by key (type-safe)
   * @tparam T Expected type
   * @param key Item key
   */
  template <typename T> [[nodiscard]] ConfigItem<T>* get_item(kj::StringPtr key) {
    std::scoped_lock lock(mu_);
    KJ_IF_SOME(item, items_.find(key)) {
      // Check type using type traits
      if (item->type() == ConfigTypeTraits<T>::type) {
        return static_cast<ConfigItem<T>*>(const_cast<ConfigItemBase*>(item.get()));
      }
    }
    return nullptr;
  }

  /**
   * @brief Get a sub-group by name
   */
  [[nodiscard]] ConfigGroup* get_group(kj::StringPtr name) {
    std::scoped_lock lock(mu_);
    KJ_IF_SOME(group, groups_.find(name)) {
      return const_cast<ConfigGroup*>(group.get());
    }
    return nullptr;
  }

  /**
   * @brief Get a sub-group by name (const version)
   */
  [[nodiscard]] const ConfigGroup* get_group(kj::StringPtr name) const {
    std::scoped_lock lock(mu_);
    KJ_IF_SOME(group, groups_.find(name)) {
      return group.get();
    }
    return nullptr;
  }

  /**
   * @brief Get all items
   */
  [[nodiscard]] kj::Vector<ConfigItemBase*> get_items() const {
    std::scoped_lock lock(mu_);
    kj::Vector<ConfigItemBase*> result;
    result.reserve(items_.size());
    for (const auto& entry : items_) {
      result.add(const_cast<ConfigItemBase*>(entry.value.get()));
    }
    return result;
  }

  /**
   * @brief Get all groups
   */
  [[nodiscard]] kj::Vector<ConfigGroup*> get_groups() const {
    std::scoped_lock lock(mu_);
    kj::Vector<ConfigGroup*> result;
    result.reserve(groups_.size());
    for (const auto& entry : groups_) {
      result.add(const_cast<ConfigGroup*>(entry.value.get()));
    }
    return result;
  }

  /**
   * @brief Validate all items in this group
   * @return true if all required items are set, false otherwise
   */
  [[nodiscard]] bool validate() const {
    std::scoped_lock lock(mu_);
    for (const auto& entry : items_) {
      if (entry.value->is_required() && !entry.value->is_set()) {
        return false;
      }
    }
    return true;
  }

  /**
   * @brief Get validation errors
   */
  [[nodiscard]] kj::Vector<kj::String> validation_errors() const {
    std::scoped_lock lock(mu_);
    kj::Vector<kj::String> errors;
    for (const auto& entry : items_) {
      if (entry.value->is_required() && !entry.value->is_set()) {
        errors.add(kj::str("Required config item '"_kj, entry.value->key(), "' is not set"_kj));
      }
    }
    return errors;
  }

  [[nodiscard]] kj::StringPtr name() const noexcept {
    return name_;
  }
  [[nodiscard]] kj::StringPtr description() const noexcept {
    return description_;
  }

private:
  kj::String name_;
  kj::String description_;
  kj::HashMap<kj::String, kj::Own<ConfigItemBase>> items_;
  kj::HashMap<kj::String, kj::Own<ConfigGroup>> groups_;
  mutable std::mutex mu_; // std::mutex for compatibility with std::scoped_lock
};

/**
 * @brief Hot reload callback type
 */
using HotReloadCallback = kj::Function<void()>;

/**
 * @brief Configuration manager
 *
 * Centralized configuration management with hot reload support.
 * Manages configuration groups and provides file-based loading.
 */
class ConfigManager final {
public:
  explicit ConfigManager(kj::StringPtr name = "default"_kj) : name_(kj::str(name)) {
    // Uses kj::heap for ownership
    root_group_ = kj::heap<ConfigGroup>("root"_kj, "Root configuration group"_kj);
  }

  ~ConfigManager() = default;

  // Disable copy and move
  ConfigManager(const ConfigManager&) = delete;
  ConfigManager& operator=(const ConfigManager&) = delete;

  /**
   * @brief Get root configuration group (const)
   */
  [[nodiscard]] const ConfigGroup* root_group() const {
    return root_group_.get();
  }

  /**
   * @brief Get root configuration group (mutable)
   */
  [[nodiscard]] ConfigGroup* root_group() {
    return root_group_.get();
  }

  /**
   * @brief Set hot reload enabled
   */
  void set_hot_reload_enabled(bool enabled) {
    hot_reload_enabled_.store(enabled);
  }

  /**
   * @brief Check if hot reload is enabled
   */
  [[nodiscard]] bool hot_reload_enabled() const {
    return hot_reload_enabled_.load();
  }

  /**
   * @brief Load configuration from JSON file
   * @param file_path Path to JSON file
   * @param reload Whether this is a reload (triggers callbacks)
   */
  bool load_from_json(const kj::Path& file_path, bool reload = false);

  /**
   * @brief Load configuration from JSON string
   */
  bool load_from_json_string(std::string_view json_content, bool reload = false);

  /**
   * @brief Load configuration from YAML file
   * @param file_path Path to YAML file
   */
  bool load_from_yaml(const kj::Path& file_path, bool reload = false);

  /**
   * @brief Save configuration to JSON file
   * @param file_path Path to save JSON file
   */
  bool save_to_json(const kj::Path& file_path) const;

  /**
   * @brief Export configuration to JSON string
   */
  [[nodiscard]] kj::String to_json() const;

  /**
   * @brief Find a config item by path (e.g., "group.subgroup.item")
   */
  [[nodiscard]] ConfigItemBase* find_item(kj::StringPtr path) const;

  /**
   * @brief Find a config item by path (templated, type-safe without dynamic_cast)
   */
  template <typename T> [[nodiscard]] ConfigItem<T>* find_item(kj::StringPtr path) {
    ConfigItemBase* base = find_item(path);
    if (base && base->type() == ConfigTypeTraits<T>::type) {
      return static_cast<ConfigItem<T>*>(base);
    }
    return nullptr;
  }

  /**
   * @brief Validate all configuration
   */
  [[nodiscard]] bool validate() const;

  /**
   * @brief Get validation errors
   */
  [[nodiscard]] kj::Vector<kj::String> validation_errors() const;

  /**
   * @brief Add hot reload callback
   */
  void add_hot_reload_callback(HotReloadCallback callback);

  /**
   * @brief Trigger hot reload callbacks
   */
  void trigger_hot_reload();

  /**
   * @brief Get configuration file path being monitored
   */
  [[nodiscard]] kj::Maybe<kj::Path> config_file() const {
    std::scoped_lock lock(mu_);
    return config_file_.map([](const kj::Path& p) { return p.clone(); });
  }

  /**
   * @brief Set configuration file for monitoring
   */
  void set_config_file(const kj::Path& file_path);

private:
  void apply_json_value(kj::StringPtr key, const veloz::core::JsonValue& value);
  static std::string item_to_json(const ConfigItemBase* item);
  static void group_to_json(const ConfigGroup* group, veloz::core::JsonBuilder& builder);

  kj::String name_;
  kj::Own<ConfigGroup> root_group_;
  kj::Maybe<kj::Path> config_file_; // kj::Path for file path representation
  std::atomic<bool> hot_reload_enabled_{false};
  kj::Vector<HotReloadCallback> hot_reload_callbacks_;
  mutable std::mutex mu_;
};

} // namespace veloz::core
