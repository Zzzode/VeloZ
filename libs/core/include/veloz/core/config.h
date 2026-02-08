#pragma once

#include <string>
#include <map>
#include <vector>
#include <variant>
#include <optional>
#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace veloz::core {

class Config final {
public:
  using Value = std::variant<bool, int64_t, double, std::string,
                             std::vector<bool>, std::vector<int64_t>,
                             std::vector<double>, std::vector<std::string>>;

  Config() = default;
  explicit Config(const std::filesystem::path& file_path);
  explicit Config(std::string_view json_content);

  bool load_from_file(const std::filesystem::path& file_path);
  bool load_from_string(std::string_view json_content);
  bool save_to_file(const std::filesystem::path& file_path) const;
  std::string to_string() const;

  // 基础访问方法
  [[nodiscard]] bool has_key(std::string_view key) const;
  void set(std::string_view key, Value value);
  void remove(std::string_view key);

  // 模板访问方法
  template <typename T>
  [[nodiscard]] std::optional<T> get(std::string_view key) const {
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

  template <typename T>
  T get_or(std::string_view key, T default_value) const {
    auto opt = get<T>(key);
    return opt.value_or(std::move(default_value));
  }

  // 嵌套配置访问
  [[nodiscard]] std::optional<Config> get_section(std::string_view key) const;
  void set_section(std::string_view key, const Config& config);

  // 配置合并
  void merge(const Config& other);

  // 获取所有键
  [[nodiscard]] std::vector<std::string> keys() const;

  // 检查是否为空
  [[nodiscard]] bool empty() const { return config_.empty(); }

  // 获取配置大小
  [[nodiscard]] size_t size() const { return config_.size(); }

private:
  std::map<std::string, Value> config_;
};

class ConfigException : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

// 全局配置访问器
[[nodiscard]] Config& global_config();
bool load_global_config(const std::filesystem::path& file_path);
bool load_global_config(std::string_view json_content);

} // namespace veloz::core
