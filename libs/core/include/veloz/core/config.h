#pragma once

#include "veloz/core/error.h" // VeloZException base class

#include <kj/array.h>
#include <kj/common.h>
#include <kj/filesystem.h> // kj::Path for file paths
#include <kj/map.h>
#include <kj/memory.h> // kj::Maybe used for nullable config values
#include <kj/one-of.h>
#include <kj/string.h>
#include <source_location> // std::source_location for exception tracking
#include <type_traits>

namespace veloz::core {

class Config final {
public:
  using Value = kj::OneOf<bool, int64_t, double, kj::String, kj::Array<bool>, kj::Array<int64_t>,
                          kj::Array<double>, kj::Array<kj::String>>;

  Config() = default;
  explicit Config(const kj::Path& file_path);
  explicit Config(kj::StringPtr json_content);

  bool load_from_file(const kj::Path& file_path);
  bool load_from_string(kj::StringPtr json_content);
  bool save_to_file(const kj::Path& file_path) const;
  kj::String to_string() const;

  // Basic access methods
  [[nodiscard]] bool has_key(kj::StringPtr key) const;
  void set(kj::StringPtr key, Value value);
  void remove(kj::StringPtr key);

  // Template access methods
  template <typename T> [[nodiscard]] kj::Maybe<T> get(kj::StringPtr key) const {
    KJ_IF_SOME(value, config_.find(key)) {
      if constexpr (std::is_same_v<T, bool>) {
        if (value.is<bool>()) {
          return value.get<bool>();
        }
      } else if constexpr (std::is_same_v<T, int64_t>) {
        if (value.is<int64_t>()) {
          return value.get<int64_t>();
        }
      } else if constexpr (std::is_same_v<T, double>) {
        if (value.is<double>()) {
          return value.get<double>();
        }
      } else if constexpr (std::is_same_v<T, kj::StringPtr>) {
        if (value.is<kj::String>()) {
          return value.get<kj::String>().asPtr();
        }
      } else if constexpr (std::is_same_v<T, kj::ArrayPtr<const bool>>) {
        if (value.is<kj::Array<bool>>()) {
          return value.get<kj::Array<bool>>().asPtr();
        }
      } else if constexpr (std::is_same_v<T, kj::ArrayPtr<const int64_t>>) {
        if (value.is<kj::Array<int64_t>>()) {
          return value.get<kj::Array<int64_t>>().asPtr();
        }
      } else if constexpr (std::is_same_v<T, kj::ArrayPtr<const double>>) {
        if (value.is<kj::Array<double>>()) {
          return value.get<kj::Array<double>>().asPtr();
        }
      } else if constexpr (std::is_same_v<T, kj::ArrayPtr<const kj::String>>) {
        if (value.is<kj::Array<kj::String>>()) {
          return value.get<kj::Array<kj::String>>().asPtr();
        }
      } else {
        static_assert(kj::isSameType<T, void>(), "Unsupported config get<T>() type");
      }
      return kj::none;
    }
    return kj::none;
  }

  template <typename T> T get_or(kj::StringPtr key, T default_value) const {
    KJ_IF_SOME(value, get<T>(key)) {
      return kj::mv(value);
    }
    return kj::mv(default_value);
  }

  // Nested configuration access
  [[nodiscard]] kj::Maybe<Config> get_section(kj::StringPtr key) const;
  void set_section(kj::StringPtr key, const Config& config);

  // Configuration merging
  void merge(const Config& other);

  // Get all keys
  [[nodiscard]] kj::Array<kj::String> keys() const;

  // Check if empty
  [[nodiscard]] bool empty() const {
    return config_.size() == 0;
  }

  // Get configuration size
  [[nodiscard]] size_t size() const {
    return config_.size();
  }

private:
  kj::TreeMap<kj::String, Value> config_;
};

/**
 * @brief Configuration-related exception using KJ exception infrastructure
 */
class ConfigException : public VeloZException {
public:
  explicit ConfigException(kj::StringPtr message,
                           const std::source_location& location = std::source_location::current())
      : VeloZException(message, kj::Exception::Type::FAILED, location) {}
};

// Global configuration accessor
[[nodiscard]] Config& global_config();
bool load_global_config(const kj::Path& file_path);
bool load_global_config(kj::StringPtr json_content);

} // namespace veloz::core
