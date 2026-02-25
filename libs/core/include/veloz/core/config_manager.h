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
#include <kj/debug.h>
#include <kj/filesystem.h> // kj::Path for file paths
#include <kj/function.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/one-of.h>
#include <kj/string.h>
#include <kj/vector.h>

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
 * Uses kj::OneOf to follow the project's KJ-first convention.
 */
using ConfigValue =
    kj::OneOf<bool, int, int64_t, double, kj::String, kj::Vector<bool>, kj::Vector<int>,
              kj::Vector<int64_t>, kj::Vector<double>, kj::Vector<kj::String>>;

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
     * Uses kj::heap<T>() and kj::Own<T> for KJ-native memory management.
     */
    kj::Own<ConfigItem<T>> build() {
      // Create with initial state
      auto item =
          kj::heap<ConfigItem<T>>(kj::mv(key_), kj::mv(description_), required_, has_default_);

      // Initialize state under lock (though single threaded here, it's the correct way to access
      // MutexGuarded)
      auto state = item->state_.lockExclusive();

      if (has_default_) {
        item->default_value_ = kj::mv(default_);
        state->value = ConfigItem<T>::clone_value(item->default_value_);
      }

      item->validator_ = kj::mv(validator_);
      state->on_change_callbacks = kj::mv(on_change_callbacks_);

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
        has_default_(has_default), state_(MutableState{T{}, has_default}) {}

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
    return state_.lockShared()->is_set;
  }
  [[nodiscard]] bool has_default() const noexcept override {
    return has_default_;
  }

  /**
   * @brief Get current value
   * @return kj::Maybe value, kj::none if not set
   */
  [[nodiscard]] kj::Maybe<T> get() const {
    auto state = state_.lockShared();
    return state->is_set ? kj::Maybe<T>(clone_value(state->value)) : kj::none;
  }

  /**
   * @brief Get value or a default
   * @param default_value Default to return if not set
   */
  [[nodiscard]] T get_or(T default_value) const {
    auto state = state_.lockShared();
    return state->is_set ? clone_value(state->value) : kj::mv(default_value);
  }

  /**
   * @brief Get value or throw if not set
   */
  [[nodiscard]] T value() const {
    auto state = state_.lockShared();
    if (!state->is_set) {
      KJ_FAIL_REQUIRE("Config item not set and has no default", kj::str(key_, " (value() called)"));
    }
    return clone_value(state->value);
  }

  /**
   * @brief Get default value
   */
  [[nodiscard]] kj::Maybe<T> default_value() const {
    // default_value_ is immutable, no lock needed?
    // Wait, default_value_ is member of ConfigItem, not MutableState.
    // It is set in Builder (single thread) and then read-only?
    // reset() reads it.
    // So it should be safe to read without lock if it's effectively const.
    // However, to be consistent with clone_value, let's just clone it.
    return has_default_ ? kj::Maybe<T>(clone_value(default_value_)) : kj::none;
  }

  /**
   * @brief Set value
   * @param value New value to set
   * @return true if successful, false if validation fails
   */
  bool set(const T& value) {
    auto state = state_.lockExclusive();
    return set_locked(state, value);
  }

  /**
   * @brief Set value (move version)
   */
  bool set(T&& value) {
    auto state = state_.lockExclusive();
    return set_locked(state, kj::mv(value));
  }

  void reset() override {
    auto state = state_.lockExclusive();
    if (has_default_) {
      state->value = clone_value(default_value_);
      state->is_set = true;
    } else {
      state->is_set = false;
    }
  }

  [[nodiscard]] kj::String to_string() const override {
    auto state = state_.lockShared();
    if constexpr (std::is_same_v<T, bool>) {
      return state->is_set ? (state->value ? kj::str("true"_kj) : kj::str("false"_kj))
                           : kj::str("not set"_kj);
    } else if constexpr (std::is_same_v<T, kj::String>) {
      return state->is_set ? kj::str("\"", state->value.cStr(), "\"") : kj::str("not set"_kj);
    } else if constexpr (std::is_same_v<T, kj::Vector<bool>> ||
                         std::is_same_v<T, kj::Vector<int>> ||
                         std::is_same_v<T, kj::Vector<int64_t>> ||
                         std::is_same_v<T, kj::Vector<double>> ||
                         std::is_same_v<T, kj::Vector<kj::String>>) {
      if (!state->is_set)
        return kj::str("not set"_kj);
      return array_to_string(state->value);
    } else {
      return state->is_set ? kj::str(state->value) : kj::str("not set"_kj);
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
      KJ_IF_SOME(parsed, value.tryParseAs<int>()) {
        return set(parsed);
      }
      return false;
    } else if constexpr (std::is_same_v<T, int64_t>) {
      KJ_IF_SOME(parsed, value.tryParseAs<long long>()) {
        return set(static_cast<int64_t>(parsed));
      }
      return false;
    } else if constexpr (std::is_same_v<T, double>) {
      KJ_IF_SOME(parsed, value.tryParseAs<double>()) {
        return set(parsed);
      }
      return false;
    } else if constexpr (std::is_same_v<T, kj::String>) {
      return set(kj::heapString(value.cStr(), value.size()));
    } else if constexpr (std::is_same_v<T, kj::Vector<bool>> ||
                         std::is_same_v<T, kj::Vector<int>> ||
                         std::is_same_v<T, kj::Vector<int64_t>> ||
                         std::is_same_v<T, kj::Vector<double>> ||
                         std::is_same_v<T, kj::Vector<kj::String>>) {
      try {
        auto j = veloz::core::JsonDocument::parse(value);
        if (j.root().is_array()) {
          // Parse array type
          if constexpr (std::is_same_v<T, kj::Vector<bool>>) {
            kj::Vector<bool> vec;
            vec.reserve(j.root().size());
            j.root().for_each_array(
                [&vec](const veloz::core::JsonValue& val) { vec.add(val.get_bool()); });
            return set(kj::mv(vec));
          } else if constexpr (std::is_same_v<T, kj::Vector<int>>) {
            kj::Vector<int> vec;
            vec.reserve(j.root().size());
            j.root().for_each_array([&vec](const veloz::core::JsonValue& val) {
              vec.add(static_cast<int>(val.get_int()));
            });
            return set(kj::mv(vec));
          } else if constexpr (std::is_same_v<T, kj::Vector<int64_t>>) {
            kj::Vector<int64_t> vec;
            vec.reserve(j.root().size());
            j.root().for_each_array(
                [&vec](const veloz::core::JsonValue& val) { vec.add(val.get_int()); });
            return set(kj::mv(vec));
          } else if constexpr (std::is_same_v<T, kj::Vector<double>>) {
            kj::Vector<double> vec;
            vec.reserve(j.root().size());
            j.root().for_each_array(
                [&vec](const veloz::core::JsonValue& val) { vec.add(val.get_double()); });
            return set(kj::mv(vec));
          } else if constexpr (std::is_same_v<T, kj::Vector<kj::String>>) {
            kj::Vector<kj::String> vec;
            vec.reserve(j.root().size());
            j.root().for_each_array(
                [&vec](const veloz::core::JsonValue& val) { vec.add(val.get_string()); });
            return set(kj::mv(vec));
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
    state_.lockExclusive()->on_change_callbacks.add(kj::mv(callback));
  }

  /**
   * @brief Convert item value to JSON value string (for serialization)
   */
  [[nodiscard]] kj::String to_json_string() const override {
    auto state = state_.lockShared();
    if (!state->is_set) {
      return kj::str("null"_kj);
    }

    if constexpr (std::is_same_v<T, kj::Vector<bool>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (bool item : state->value) {
        arr_builder.add(item);
      }
      return arr_builder.build();
    } else if constexpr (std::is_same_v<T, kj::Vector<int>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (int item : state->value) {
        arr_builder.add(item);
      }
      return arr_builder.build();
    } else if constexpr (std::is_same_v<T, kj::Vector<int64_t>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (int64_t item : state->value) {
        arr_builder.add(item);
      }
      return arr_builder.build();
    } else if constexpr (std::is_same_v<T, kj::Vector<double>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (double item : state->value) {
        arr_builder.add(item);
      }
      return arr_builder.build();
    } else if constexpr (std::is_same_v<T, kj::Vector<kj::String>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (const kj::String& item : state->value) {
        arr_builder.add(item);
      }
      return arr_builder.build();
    } else if constexpr (std::is_same_v<T, kj::String>) {
      return kj::str("\"", state->value.cStr(), "\"");
    } else {
      veloz::core::JsonBuilder builder = veloz::core::JsonBuilder::object();
      builder.put("value"_kj, state->value);
      auto doc = veloz::core::JsonDocument::parse(builder.build());
      auto root = doc.root();

      if constexpr (std::is_same_v<T, bool>) {
        return root["value"_kj].get_bool() ? kj::str("true"_kj) : kj::str("false"_kj);
      } else if constexpr (std::is_integral_v<T>) {
        return kj::str(root["value"_kj].get_int());
      } else if constexpr (std::is_floating_point_v<T>) {
        return kj::str(root["value"_kj].get_double());
      }
    }

    return kj::str("null"_kj);
  }

private:
  [[nodiscard]] static bool equals(const T& a, const T& b) {
    if constexpr (std::is_same_v<T, kj::String>) {
      return a.asPtr() == b.asPtr();
    } else if constexpr (std::is_same_v<T, kj::Vector<kj::String>>) {
      if (a.size() != b.size()) {
        return false;
      }
      for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].asPtr() != b[i].asPtr()) {
          return false;
        }
      }
      return true;
    } else if constexpr (std::is_same_v<T, kj::Vector<bool>> ||
                         std::is_same_v<T, kj::Vector<int>> ||
                         std::is_same_v<T, kj::Vector<int64_t>> ||
                         std::is_same_v<T, kj::Vector<double>>) {
      if (a.size() != b.size()) {
        return false;
      }
      for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
          return false;
        }
      }
      return true;
    } else {
      return a == b;
    }
  }

  [[nodiscard]] static T clone_value(const T& value) {
    if constexpr (std::is_same_v<T, kj::String>) {
      return kj::heapString(value.cStr(), value.size());
    } else if constexpr (std::is_same_v<T, kj::Vector<kj::String>>) {
      kj::Vector<kj::String> out;
      out.reserve(value.size());
      for (const auto& item : value) {
        out.add(kj::heapString(item.cStr(), item.size()));
      }
      return out;
    } else if constexpr (std::is_same_v<T, kj::Vector<bool>> ||
                         std::is_same_v<T, kj::Vector<int>> ||
                         std::is_same_v<T, kj::Vector<int64_t>> ||
                         std::is_same_v<T, kj::Vector<double>>) {
      T out;
      out.reserve(value.size());
      for (const auto& item : value) {
        out.add(item);
      }
      return out;
    } else {
      return value;
    }
  }

  template <typename StatePtr, typename U> bool set_locked(StatePtr& state, U&& value) {
    KJ_IF_SOME(v, validator_) {
      if (!v(value)) {
        return false;
      }
    }

    bool was_set = state->is_set;
    T old_value = was_set ? clone_value(state->value) : T{};
    if constexpr (std::is_same_v<T, kj::String> || std::is_same_v<T, kj::Vector<bool>> ||
                  std::is_same_v<T, kj::Vector<int>> || std::is_same_v<T, kj::Vector<int64_t>> ||
                  std::is_same_v<T, kj::Vector<double>> ||
                  std::is_same_v<T, kj::Vector<kj::String>>) {
      if constexpr (std::is_rvalue_reference_v<U&&>) {
        state->value = kj::mv(value);
      } else {
        state->value = clone_value(value);
      }
    } else {
      state->value = std::forward<U>(value);
    }
    state->is_set = true;

    // Notify callbacks if value changed or was previously unset
    if (!was_set || !equals(old_value, state->value)) {
      for (auto& callback : state->on_change_callbacks) {
        callback(old_value, state->value);
      }
    }

    return true;
  }

  [[nodiscard]] static kj::String array_to_string(const kj::Vector<bool>& v) {
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

  template <typename V> [[nodiscard]] static kj::String array_to_string(const kj::Vector<V>& v) {
    kj::Vector<kj::String> parts;
    parts.reserve(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
      if constexpr (std::is_same_v<V, kj::String>) {
        parts.add(kj::str("\"", v[i].cStr(), "\""));
      } else {
        parts.add(kj::str(v[i]));
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

  struct MutableState {
    T value;
    bool is_set;
    kj::Vector<ConfigChangeCallback<T>> on_change_callbacks;
  };

  kj::String key_;
  kj::String description_;
  T default_value_;
  // T value_; // Moved to MutableState
  bool required_;
  bool has_default_;
  kj::Maybe<ConfigValidator<T>> validator_;
  kj::MutexGuarded<MutableState> state_;
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

template <> struct ConfigTypeTraits<kj::String> {
  static constexpr ConfigItemType type = ConfigItemType::String;
};

template <> struct ConfigTypeTraits<kj::Vector<bool>> {
  static constexpr ConfigItemType type = ConfigItemType::BoolArray;
};

template <> struct ConfigTypeTraits<kj::Vector<int>> {
  static constexpr ConfigItemType type = ConfigItemType::IntArray;
};

template <> struct ConfigTypeTraits<kj::Vector<int64_t>> {
  static constexpr ConfigItemType type = ConfigItemType::Int64Array;
};

template <> struct ConfigTypeTraits<kj::Vector<double>> {
  static constexpr ConfigItemType type = ConfigItemType::DoubleArray;
};

template <> struct ConfigTypeTraits<kj::Vector<kj::String>> {
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
    auto key_view = item->key();
    kj::String key_str = kj::heapString(key_view);
    state_.lockExclusive()->items.upsert(
        kj::mv(key_str), kj::mv(item),
        [](auto& existing, auto&& newVal) { existing = kj::mv(newVal); });
  }

  /**
   * @brief Add a sub-group
   * @param group Group to add (takes ownership)
   */
  void add_group(kj::Own<ConfigGroup> group) {
    kj::String name_str = kj::heapString(group->name_);
    state_.lockExclusive()->groups.upsert(
        kj::mv(name_str), kj::mv(group),
        [](auto& existing, auto&& newVal) { existing = kj::mv(newVal); });
  }

  /**
   * @brief Get a config item by key (type-safe)
   * @tparam T Expected type
   * @param key Item key
   */
  template <typename T> [[nodiscard]] ConfigItem<T>* get_item(kj::StringPtr key) {
    auto state = state_.lockShared();
    KJ_IF_SOME(item, state->items.find(key)) {
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
    auto state = state_.lockShared();
    KJ_IF_SOME(group, state->groups.find(name)) {
      return const_cast<ConfigGroup*>(group.get());
    }
    return nullptr;
  }

  /**
   * @brief Get a sub-group by name (const version)
   */
  [[nodiscard]] const ConfigGroup* get_group(kj::StringPtr name) const {
    auto state = state_.lockShared();
    KJ_IF_SOME(group, state->groups.find(name)) {
      return group.get();
    }
    return nullptr;
  }

  /**
   * @brief Get all items
   */
  [[nodiscard]] kj::Vector<ConfigItemBase*> get_items() const {
    auto state = state_.lockShared();
    kj::Vector<ConfigItemBase*> result;
    result.reserve(state->items.size());
    for (const auto& entry : state->items) {
      result.add(const_cast<ConfigItemBase*>(entry.value.get()));
    }
    return result;
  }

  /**
   * @brief Get all groups
   */
  [[nodiscard]] kj::Vector<ConfigGroup*> get_groups() const {
    auto state = state_.lockShared();
    kj::Vector<ConfigGroup*> result;
    result.reserve(state->groups.size());
    for (const auto& entry : state->groups) {
      result.add(const_cast<ConfigGroup*>(entry.value.get()));
    }
    return result;
  }

  /**
   * @brief Validate all items in this group
   * @return true if all required items are set, false otherwise
   */
  [[nodiscard]] bool validate() const {
    auto state = state_.lockShared();
    for (const auto& entry : state->items) {
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
    auto state = state_.lockShared();
    kj::Vector<kj::String> errors;
    for (const auto& entry : state->items) {
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
  struct MutableState {
    kj::HashMap<kj::String, kj::Own<ConfigItemBase>> items;
    kj::HashMap<kj::String, kj::Own<ConfigGroup>> groups;
  };

  kj::String name_;
  kj::String description_;
  kj::MutexGuarded<MutableState> state_;
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
  bool load_from_json_string(kj::StringPtr json_content, bool reload = false);

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
    auto state = state_.lockShared();
    return state->config_file.map([](const kj::Path& p) { return p.clone(); });
  }

  /**
   * @brief Set configuration file for monitoring
   */
  void set_config_file(const kj::Path& file_path);

private:
  void apply_json_value(kj::StringPtr key, const veloz::core::JsonValue& value);

  struct MutableState {
    kj::Maybe<kj::Path> config_file;
    kj::Vector<HotReloadCallback> hot_reload_callbacks;
  };

  kj::String name_;
  kj::Own<ConfigGroup> root_group_;
  std::atomic<bool> hot_reload_enabled_{false};
  kj::MutexGuarded<MutableState> state_;
};

} // namespace veloz::core
