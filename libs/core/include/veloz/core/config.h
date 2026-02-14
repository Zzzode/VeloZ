#pragma once

#include <filesystem>
#include <kj/common.h>
#include <kj/string.h>
#include <map>      // std::map used for ordered key iteration
#include <optional> // std::optional used for nullable config values
#include <stdexcept>
#include <string> // std::string used for std::map key and std::variant compatibility
#include <string_view>
#include <variant>
#include <vector> // std::vector used for array config values in std::variant

namespace veloz::core {

class Config final {
public:
  using Value = std::variant<bool, int64_t, double, std::string, std::vector<bool>,
                             std::vector<int64_t>, std::vector<double>, std::vector<std::string>>;

  Config() = default;
  explicit Config(const std::filesystem::path& file_path);
  explicit Config(std::string_view json_content);

  bool load_from_file(const std::filesystem::path& file_path);
  bool load_from_string(std::string_view json_content);
  bool save_to_file(const std::filesystem::path& file_path) const;
  std::string to_string() const;

  // Basic access methods
  [[nodiscard]] bool has_key(std::string_view key) const;
  void set(std::string_view key, Value value);
  void remove(std::string_view key);

  // Template access methods
  template <typename T> [[nodiscard]] std::optional<T> get(std::string_view key) const {
    auto it = config_.find(std::string(key));
    if (it == config_.end()) {
      return std::nullopt;
    }
    try {
      if constexpr (std::is_same_v<T, bool>) {
        return std::get<bool>(it->second);
      } else if constexpr (std::is_same_v<T, int64_t>) {
        return std::get<int64_t>(it->second);
      } else if constexpr (std::is_same_v<T, double>) {
        return std::get<double>(it->second);
      } else if constexpr (std::is_same_v<T, std::string>) {
        return std::get<std::string>(it->second);
      } else if constexpr (std::is_same_v<T, std::vector<bool>>) {
        return std::get<std::vector<bool>>(it->second);
      } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
        return std::get<std::vector<int64_t>>(it->second);
      } else if constexpr (std::is_same_v<T, std::vector<double>>) {
        return std::get<std::vector<double>>(it->second);
      } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
        return std::get<std::vector<std::string>>(it->second);
      }
    } catch (const std::bad_variant_access&) {
      return std::nullopt;
    }
    return std::nullopt;
  }

  template <typename T> T get_or(std::string_view key, T default_value) const {
    auto opt = get<T>(key);
    return opt.value_or(kj::mv(default_value));
  }

  // Nested configuration access
  [[nodiscard]] std::optional<Config> get_section(std::string_view key) const;
  void set_section(std::string_view key, const Config& config);

  // Configuration merging
  void merge(const Config& other);

  // Get all keys
  [[nodiscard]] std::vector<std::string> keys() const;

  // Check if empty
  [[nodiscard]] bool empty() const {
    return config_.empty();
  }

  // Get configuration size
  [[nodiscard]] size_t size() const {
    return config_.size();
  }

private:
  std::map<std::string, Value> config_;
};

class ConfigException : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

// Global configuration accessor
[[nodiscard]] Config& global_config();
bool load_global_config(const std::filesystem::path& file_path);
bool load_global_config(std::string_view json_content);

} // namespace veloz::core
