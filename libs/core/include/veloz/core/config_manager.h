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
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

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
 */
using ConfigValue =
    std::variant<bool, int, int64_t, double, std::string, std::vector<bool>, std::vector<int>,
                 std::vector<int64_t>, std::vector<double>, std::vector<std::string>>;

/**
 * @brief Validation function signature
 *
 * Used to validate configuration values before they are set.
 */
template <typename T> using ConfigValidator = std::function<bool(const T&)>;

/**
 * @brief Change callback function signature
 *
 * Called when a configuration value changes.
 */
template <typename T>
using ConfigChangeCallback = std::function<void(const T& old_value, const T& new_value)>;

/**
 * @brief Base configuration item interface
 *
 * Provides type-erased access to configuration items.
 */
class ConfigItemBase {
public:
  virtual ~ConfigItemBase() = default;

  /**
   * @brief Get the key/name of this configuration item
   */
  [[nodiscard]] virtual std::string_view key() const noexcept = 0;

  /**
   * @brief Get the description of this configuration item
   */
  [[nodiscard]] virtual std::string_view description() const noexcept = 0;

  /**
   * @brief Get the type of this configuration item
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
  [[nodiscard]] virtual std::string to_string() const = 0;

  /**
   * @brief Set value from a string
   */
  virtual bool from_string(std::string_view value) = 0;

  /**
   * @brief Convert item value to JSON value string (for serialization)
   */
  [[nodiscard]] virtual std::string to_json_string() const = 0;
};

/**
 * @brief Type-safe configuration item
 *
 * Represents a single configuration value with type safety,
 * validation, and change notification support.
 *
 * @tparam T The type of the configuration value
 */
template <typename T> class ConfigItem : public ConfigItemBase {
public:
  /**
   * @brief Builder for ConfigItem
   */
  class Builder {
  public:
    Builder(std::string key, std::string description)
        : key_(std::move(key)), description_(std::move(description)) {}

    /**
     * @brief Set the default value
     */
    Builder& default_value(T value) {
      default_ = std::move(value);
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
      validator_ = std::move(validator);
      return *this;
    }

    /**
     * @brief Add a change callback
     */
    Builder& on_change(ConfigChangeCallback<T> callback) {
      on_change_callbacks_.push_back(std::move(callback));
      return *this;
    }

    /**
     * @brief Build the ConfigItem
     */
    std::unique_ptr<ConfigItem<T>> build() {
      auto item = std::make_unique<ConfigItem<T>>(std::move(key_), std::move(description_),
                                                  required_, has_default_);

      if (has_default_) {
        item->default_value_ = std::move(default_);
        item->value_ = item->default_value_;
      }

      item->validator_ = std::move(validator_);
      item->on_change_callbacks_ = std::move(on_change_callbacks_);

      return item;
    }

  private:
    std::string key_;
    std::string description_;
    T default_{};
    bool has_default_{false};
    bool required_{false};
    ConfigValidator<T> validator_;
    std::vector<ConfigChangeCallback<T>> on_change_callbacks_;
  };

  /**
   * @brief Constructor (use Builder instead)
   */
  ConfigItem(std::string key, std::string description, bool required, bool has_default)
      : key_(std::move(key)), description_(std::move(description)), required_(required),
        has_default_(has_default) {
    is_set_.store(has_default);
  }

  // Getters
  [[nodiscard]] std::string_view key() const noexcept override {
    return key_;
  }
  [[nodiscard]] std::string_view description() const noexcept override {
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
   * @brief Get the current value
   * @return Optional value, nullopt if not set
   */
  [[nodiscard]] std::optional<T> get() const {
    std::lock_guard<std::mutex> lock(mu_);
    return is_set_ ? std::optional<T>(value_) : std::nullopt;
  }

  /**
   * @brief Get the value or a default
   * @param default_value Default to return if not set
   */
  [[nodiscard]] T get_or(T default_value) const {
    std::lock_guard<std::mutex> lock(mu_);
    return is_set_ ? value_ : std::move(default_value);
  }

  /**
   * @brief Get the value or throw if not set
   */
  [[nodiscard]] const T& value() const {
    std::lock_guard<std::mutex> lock(mu_);
    if (!is_set_) {
      throw std::runtime_error("Config item '" + key_ + "' is not set and has no default");
    }
    return value_;
  }

  /**
   * @brief Get the default value
   */
  [[nodiscard]] std::optional<T> default_value() const {
    std::lock_guard<std::mutex> lock(mu_);
    return has_default_ ? std::optional<T>(default_value_) : std::nullopt;
  }

  /**
   * @brief Set the value
   * @param value New value to set
   * @return true if successful, false if validation fails
   */
  bool set(const T& value) {
    std::scoped_lock lock(mu_);
    return set_locked(value);
  }

  /**
   * @brief Set the value (move version)
   */
  bool set(T&& value) {
    std::scoped_lock lock(mu_);
    return set_locked(std::move(value));
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

  [[nodiscard]] std::string to_string() const override {
    std::scoped_lock lock(mu_);
    if constexpr (std::is_same_v<T, bool>) {
      return is_set_ ? (value_ ? "true" : "false") : "not set";
    } else if constexpr (std::is_same_v<T, std::string>) {
      return is_set_ ? "\"" + value_ + "\"" : "not set";
    } else if constexpr (std::is_same_v<T, std::vector<bool>> ||
                         std::is_same_v<T, std::vector<int>> ||
                         std::is_same_v<T, std::vector<int64_t>> ||
                         std::is_same_v<T, std::vector<double>> ||
                         std::is_same_v<T, std::vector<std::string>>) {
      if (!is_set_)
        return "not set";
      return array_to_string(value_);
    } else {
      return is_set_ ? std::to_string(value_) : "not set";
    }
  }

  bool from_string(std::string_view value) override {
    if constexpr (std::is_same_v<T, bool>) {
      if (value == "true" || value == "1")
        return set(true);
      if (value == "false" || value == "0")
        return set(false);
      return false;
    } else if constexpr (std::is_same_v<T, int>) {
      try {
        return set(std::stoi(std::string(value)));
      } catch (...) {
        return false;
      }
    } else if constexpr (std::is_same_v<T, int64_t>) {
      try {
        return set(std::stoll(std::string(value)));
      } catch (...) {
        return false;
      }
    } else if constexpr (std::is_same_v<T, double>) {
      try {
        return set(std::stod(std::string(value)));
      } catch (...) {
        return false;
      }
    } else if constexpr (std::is_same_v<T, std::string>) {
      return set(std::string(value));
    } else if constexpr (std::is_same_v<T, std::vector<bool>> ||
                         std::is_same_v<T, std::vector<int>> ||
                         std::is_same_v<T, std::vector<int64_t>> ||
                         std::is_same_v<T, std::vector<double>> ||
                         std::is_same_v<T, std::vector<std::string>>) {
      try {
        auto j = veloz::core::JsonDocument::parse(std::string(value));
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
    on_change_callbacks_.push_back(std::move(callback));
  }

  /**
   * @brief Convert item value to JSON value string (for serialization)
   */
  [[nodiscard]] std::string to_json_string() const override {
    if (!is_set_) {
      return "null";
    }

    std::lock_guard<std::mutex> lock(mu_);
    if constexpr (std::is_same_v<T, std::vector<bool>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (bool item : value_) {
        arr_builder.add(item);
      }
      return arr_builder.build();
    } else if constexpr (std::is_same_v<T, std::vector<int>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (int item : value_) {
        arr_builder.add(item);
      }
      return arr_builder.build();
    } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (int64_t item : value_) {
        arr_builder.add(item);
      }
      return arr_builder.build();
    } else if constexpr (std::is_same_v<T, std::vector<double>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (double item : value_) {
        arr_builder.add(item);
      }
      return arr_builder.build();
    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
      veloz::core::JsonBuilder arr_builder = veloz::core::JsonBuilder::array();
      for (const std::string& item : value_) {
        arr_builder.add(item);
      }
      return arr_builder.build();
    } else {
      veloz::core::JsonBuilder builder = veloz::core::JsonBuilder::object();
      builder.put("value", value_);
      auto doc = veloz::core::JsonDocument::parse(builder.build());
      auto root = doc.root();

      if constexpr (std::is_same_v<T, bool>) {
        return root["value"].get_bool() ? "true" : "false";
      } else if constexpr (std::is_integral_v<T>) {
        return std::to_string(root["value"].get_int());
      } else if constexpr (std::is_floating_point_v<T>) {
        return std::to_string(root["value"].get_double());
      } else if constexpr (std::is_same_v<T, std::string>) {
        return "\"" + root["value"].get_string() + "\"";
      }
    }

    return "null";
  }

private:
  template <typename U> bool set_locked(U&& value) {
    if (validator_ && !validator_(value)) {
      return false;
    }

    T old_value = is_set_ ? value_ : T{};
    bool was_set = is_set_;
    value_ = std::forward<U>(value);
    is_set_ = true;

    // Notify callbacks if value changed or was previously unset
    if (!was_set || old_value != value_) {
      for (const auto& callback : on_change_callbacks_) {
        callback(old_value, value_);
      }
    }

    return true;
  }

  [[nodiscard]] static std::string array_to_string(const std::vector<bool>& v) {
    std::string s = "[";
    for (size_t i = 0; i < v.size(); ++i) {
      s += v[i] ? "true" : "false";
      if (i < v.size() - 1)
        s += ", ";
    }
    s += "]";
    return s;
  }

  template <typename V> [[nodiscard]] static std::string array_to_string(const std::vector<V>& v) {
    std::string s = "[";
    for (size_t i = 0; i < v.size(); ++i) {
      if constexpr (std::is_same_v<V, std::string>) {
        s += "\"" + v[i] + "\"";
      } else {
        s += std::to_string(v[i]);
      }
      if (i < v.size() - 1)
        s += ", ";
    }
    s += "]";
    return s;
  }

  std::string key_;
  std::string description_;
  T default_value_;
  T value_;
  bool required_;
  bool has_default_;
  std::atomic<bool> is_set_{false};
  ConfigValidator<T> validator_;
  std::vector<ConfigChangeCallback<T>> on_change_callbacks_;
  mutable std::mutex mu_;
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
  explicit ConfigGroup(std::string name, std::string description = "")
      : name_(std::move(name)), description_(std::move(description)) {}

  /**
   * @brief Add a config item to this group
   * @param item Config item to add (takes ownership)
   */
  void add_item(std::unique_ptr<ConfigItemBase> item) {
    std::scoped_lock lock(mu_);
    std::string key_str = std::string(item->key());
    items_[key_str] = std::move(item);
  }

  /**
   * @brief Add a sub-group
   * @param group Group to add (takes ownership)
   */
  void add_group(std::unique_ptr<ConfigGroup> group) {
    std::scoped_lock lock(mu_);
    groups_[group->name_] = std::move(group);
  }

  /**
   * @brief Get a config item by key (type-safe)
   * @tparam T Expected type
   * @param key Item key
   */
  template <typename T> [[nodiscard]] ConfigItem<T>* get_item(std::string_view key) {
    std::scoped_lock lock(mu_);
    auto it = items_.find(std::string(key));
    if (it == items_.end()) {
      return nullptr;
    }

    // Check type using type traits
    if (it->second->type() == ConfigTypeTraits<T>::type) {
      return static_cast<ConfigItem<T>*>(it->second.get());
    }

    return nullptr;
  }

  /**
   * @brief Get a sub-group by name
   */
  [[nodiscard]] ConfigGroup* get_group(std::string_view name) {
    std::scoped_lock lock(mu_);
    auto it = groups_.find(std::string(name));
    return it != groups_.end() ? it->second.get() : nullptr;
  }

  /**
   * @brief Get all items
   */
  [[nodiscard]] std::vector<ConfigItemBase*> get_items() const {
    std::scoped_lock lock(mu_);
    std::vector<ConfigItemBase*> result;
    result.reserve(items_.size());
    for (auto& [key, item] : items_) {
      result.push_back(item.get());
    }
    return result;
  }

  /**
   * @brief Get all groups
   */
  [[nodiscard]] std::vector<ConfigGroup*> get_groups() const {
    std::scoped_lock lock(mu_);
    std::vector<ConfigGroup*> result;
    result.reserve(groups_.size());
    for (auto& [name, group] : groups_) {
      result.push_back(group.get());
    }
    return result;
  }

  /**
   * @brief Validate all items in this group
   * @return true if all required items are set, false otherwise
   */
  [[nodiscard]] bool validate() const {
    std::scoped_lock lock(mu_);
    for (const auto& [key, item] : items_) {
      if (item->is_required() && !item->is_set()) {
        return false;
      }
    }
    return true;
  }

  /**
   * @brief Get validation errors
   */
  [[nodiscard]] std::vector<std::string> validation_errors() const {
    std::scoped_lock lock(mu_);
    std::vector<std::string> errors;
    for (const auto& [key, item] : items_) {
      if (item->is_required() && !item->is_set()) {
        errors.push_back("Required config item '" + std::string(item->key()) + "' is not set");
      }
    }
    return errors;
  }

  [[nodiscard]] std::string_view name() const noexcept {
    return name_;
  }
  [[nodiscard]] std::string_view description() const noexcept {
    return description_;
  }

private:
  std::string name_;
  std::string description_;
  std::unordered_map<std::string, std::unique_ptr<ConfigItemBase>> items_;
  std::unordered_map<std::string, std::unique_ptr<ConfigGroup>> groups_;
  mutable std::mutex mu_;
};

/**
 * @brief Hot reload callback type
 */
using HotReloadCallback = std::function<void()>;

/**
 * @brief Configuration manager
 *
 * Centralized configuration management with hot reload support.
 * Manages configuration groups and provides file-based loading.
 */
class ConfigManager final {
public:
  explicit ConfigManager(std::string name = "default") : name_(std::move(name)) {
    root_group_ = std::make_unique<ConfigGroup>("root", "Root configuration group");
  }

  ~ConfigManager() = default;

  // Disable copy and move
  ConfigManager(const ConfigManager&) = delete;
  ConfigManager& operator=(const ConfigManager&) = delete;

  /**
   * @brief Get the root configuration group
   */
  [[nodiscard]] ConfigGroup* root_group() const {
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
  bool load_from_json(const std::filesystem::path& file_path, bool reload = false);

  /**
   * @brief Load configuration from JSON string
   */
  bool load_from_json_string(std::string_view json_content, bool reload = false);

  /**
   * @brief Load configuration from YAML file
   * @param file_path Path to YAML file
   */
  bool load_from_yaml(const std::filesystem::path& file_path, bool reload = false);

  /**
   * @brief Save configuration to JSON file
   */
  bool save_to_json(const std::filesystem::path& file_path) const;

  /**
   * @brief Export configuration to JSON string
   */
  [[nodiscard]] std::string to_json() const;

  /**
   * @brief Find a config item by path (e.g., "group.subgroup.item")
   */
  [[nodiscard]] ConfigItemBase* find_item(std::string_view path) const;

  /**
   * @brief Find a config item by path (templated, type-safe without dynamic_cast)
   */
  template <typename T> [[nodiscard]] ConfigItem<T>* find_item(std::string_view path) {
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
  [[nodiscard]] std::vector<std::string> validation_errors() const;

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
  [[nodiscard]] std::filesystem::path config_file() const {
    std::scoped_lock lock(mu_);
    return config_file_;
  }

  /**
   * @brief Set configuration file for monitoring
   */
  void set_config_file(const std::filesystem::path& file_path);

private:
  void apply_json_value(const std::string& key, const veloz::core::JsonValue& value);
  static std::string item_to_json(const ConfigItemBase* item);
  static void group_to_json(const ConfigGroup* group, veloz::core::JsonBuilder& builder);

  std::string name_;
  std::unique_ptr<ConfigGroup> root_group_;
  std::filesystem::path config_file_;
  std::atomic<bool> hot_reload_enabled_{false};
  std::vector<HotReloadCallback> hot_reload_callbacks_;
  mutable std::mutex mu_;
};

} // namespace veloz::core
