#include "veloz/core/config.h"

#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using namespace nlohmann;
namespace veloz::core {

using json = nlohmann::json;

namespace {

Config::Value json_to_value(const nlohmann::json& j) {
  if (j.is_boolean()) {
    return j.get<bool>();
  } else if (j.is_number_integer()) {
    return j.get<int64_t>();
  } else if (j.is_number_float()) {
    return j.get<double>();
  } else if (j.is_string()) {
    return j.get<std::string>();
  } else if (j.is_array()) {
    if (j.empty()) {
      return std::vector<std::string>{};
    }
    const auto& first = j[0];
    if (first.is_boolean()) {
      return j.get<std::vector<bool>>();
    } else if (first.is_number_integer()) {
      return j.get<std::vector<int64_t>>();
    } else if (first.is_number_float()) {
      return j.get<std::vector<double>>();
    } else if (first.is_string()) {
      return j.get<std::vector<std::string>>();
    }
  }
  throw ConfigException("Unsupported JSON type");
}

nlohmann::json value_to_json(const Config::Value& v) {
  switch (v.index()) {
    case 0: // bool
      return std::get<bool>(v);
    case 1: // int64_t
      return std::get<int64_t>(v);
    case 2: // double
      return std::get<double>(v);
    case 3: // std::string
      return std::get<std::string>(v);
    case 4: // std::vector<bool>
      return std::get<std::vector<bool>>(v);
    case 5: // std::vector<int64_t>
      return std::get<std::vector<int64_t>>(v);
    case 6: // std::vector<double>
      return std::get<std::vector<double>>(v);
    case 7: // std::vector<std::string>
      return std::get<std::vector<std::string>>(v);
    default:
      throw ConfigException("Unsupported value type");
  }
}

} // namespace

Config::Config(const std::filesystem::path& file_path) {
  load_from_file(file_path);
}

Config::Config(std::string_view json_content) {
  load_from_string(json_content);
}

bool Config::load_from_file(const std::filesystem::path& file_path) {
  try {
    std::ifstream ifs(file_path);
    if (!ifs.is_open()) {
      throw ConfigException("Failed to open config file: " + file_path.string());
    }
    json j;
    ifs >> j;

    config_.clear();
    for (auto it = j.begin(); it != j.end(); ++it) {
      config_[it.key()] = json_to_value(it.value());
    }
    return true;
  } catch (...) {
    return false;
  }
}

bool Config::load_from_string(std::string_view json_content) {
  try {
    json j = json::parse(json_content);

    config_.clear();
    for (auto it = j.begin(); it != j.end(); ++it) {
      config_[it.key()] = json_to_value(it.value());
    }
    return true;
  } catch (...) {
    return false;
  }
}

bool Config::save_to_file(const std::filesystem::path& file_path) const {
  try {
    std::ofstream ofs(file_path);
    if (!ofs.is_open()) {
      throw ConfigException("Failed to open file for writing: " + file_path.string());
    }
    ofs << to_string();
    return true;
  } catch (...) {
    return false;
  }
}

std::string Config::to_string() const {
  json j;
  for (const auto& [key, value] : config_) {
    j[key] = value_to_json(value);
  }
  return j.dump(2);
}

bool Config::has_key(std::string_view key) const {
  return config_.contains(std::string(key));
}

void Config::set(std::string_view key, Value value) {
  config_[std::string(key)] = std::move(value);
}

void Config::remove(std::string_view key) {
  config_.erase(std::string(key));
}

std::optional<Config> Config::get_section(std::string_view key) const {
  auto it = config_.find(std::string(key));
  if (it == config_.end()) {
    return std::nullopt;
  }

  // 这里可以扩展为支持嵌套配置
  return std::nullopt;
}

void Config::set_section(std::string_view key, const Config& config) {
  // 这里可以扩展为支持嵌套配置
}

void Config::merge(const Config& other) {
  for (const auto& [key, value] : other.config_) {
    config_[key] = value;
  }
}

std::vector<std::string> Config::keys() const {
  std::vector<std::string> result;
  result.reserve(config_.size());
  for (const auto& [key, value] : config_) {
    result.push_back(key);
  }
  return result;
}

// 全局配置实例
static Config* g_global_config = nullptr;

Config& global_config() {
  if (g_global_config == nullptr) {
    g_global_config = new Config();
  }
  return *g_global_config;
}

bool load_global_config(const std::filesystem::path& file_path) {
  return global_config().load_from_file(file_path);
}

bool load_global_config(std::string_view json_content) {
  return global_config().load_from_string(json_content);
}

} // namespace veloz::core
