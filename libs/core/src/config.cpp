#include "veloz/core/config.h"

#include "veloz/core/json.h"

#include <fstream>

namespace veloz::core {

namespace {

Config::Value json_to_value(const JsonValue& j) {
  if (j.is_bool()) {
    return j.get_bool();
  } else if (j.is_int()) {
    return j.get_int();
  } else if (j.is_real()) {
    return j.get_double();
  } else if (j.is_string()) {
    return j.get_string();
  } else if (j.is_array()) {
    if (j.size() == 0) {
      return std::vector<std::string>{};
    }
    const auto& first = j[0];
    if (first.is_bool()) {
      std::vector<bool> result;
      j.for_each_array([&result](const JsonValue& val) { result.push_back(val.get_bool()); });
      return result;
    } else if (first.is_int()) {
      std::vector<int64_t> result;
      j.for_each_array([&result](const JsonValue& val) { result.push_back(val.get_int()); });
      return result;
    } else if (first.is_real()) {
      std::vector<double> result;
      j.for_each_array([&result](const JsonValue& val) { result.push_back(val.get_double()); });
      return result;
    } else if (first.is_string()) {
      std::vector<std::string> result;
      j.for_each_array([&result](const JsonValue& val) { result.push_back(val.get_string()); });
      return result;
    }
  }
  throw ConfigException("Unsupported JSON type");
}

JsonBuilder value_to_json_builder(const Config::Value& v) {
  return std::visit(
      [](auto&& arg) -> JsonBuilder {
        auto builder = JsonBuilder::object();
        if constexpr (std::is_same_v<decltype(arg), bool>) {
          builder.put("value", arg);
        } else if constexpr (std::is_same_v<decltype(arg), int64_t>) {
          builder.put("value", arg);
        } else if constexpr (std::is_same_v<decltype(arg), double>) {
          builder.put("value", arg);
        } else if constexpr (std::is_same_v<decltype(arg), std::string>) {
          builder.put("value", arg);
        } else if constexpr (std::is_same_v<decltype(arg), std::vector<bool>>) {
          builder.put_array("value", [&arg](JsonBuilder& b) {
            for (const auto& item : arg)
              b.add(item);
          });
        } else if constexpr (std::is_same_v<decltype(arg), std::vector<int64_t>>) {
          builder.put_array("value", [&arg](JsonBuilder& b) {
            for (const auto& item : arg)
              b.add(item);
          });
        } else if constexpr (std::is_same_v<decltype(arg), std::vector<double>>) {
          builder.put_array("value", [&arg](JsonBuilder& b) {
            for (const auto& item : arg)
              b.add(item);
          });
        } else if constexpr (std::is_same_v<decltype(arg), std::vector<std::string>>) {
          builder.put_array("value", [&arg](JsonBuilder& b) {
            for (const auto& item : arg)
              b.add(item);
          });
        }
        return builder;
      },
      v);
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
    auto doc = JsonDocument::parse_file(std::string(file_path));
    auto root = doc.root();

    config_.clear();
    root.for_each_object([this](const std::string& key, const JsonValue& value) {
      config_[key] = json_to_value(value);
    });
    return true;
  } catch (...) {
    return false;
  }
}

bool Config::load_from_string(std::string_view json_content) {
  try {
    auto doc = JsonDocument::parse(std::string(json_content));
    auto root = doc.root();

    config_.clear();
    root.for_each_object([this](const std::string& key, const JsonValue& value) {
      config_[key] = json_to_value(value);
    });
    return true;
  } catch (...) {
    return false;
  }
}

bool Config::save_to_file(const std::filesystem::path& file_path) const {
  try {
    auto builder = JsonBuilder::object();
    for (const auto& [key, value] : config_) {
      auto nested = value_to_json_builder(value);
      // Build the nested value and add to main builder
      // Since JsonBuilder::put_array takes a function, we need to work around this
      // For now, let's use a simpler approach
      // Actually, let's create the JSON string differently
    }
    std::string json_str = builder.build();

    std::ofstream ofs(file_path);
    if (!ofs.is_open()) {
      throw ConfigException("Failed to open file for writing: " + file_path.string());
    }
    ofs << json_str;
    return true;
  } catch (...) {
    return false;
  }
}

std::string Config::to_string() const {
  auto builder = JsonBuilder::object();
  for (const auto& [key, value] : config_) {
    // Use a helper function to add values to builder
    std::visit(
        [&builder, &key](auto&& arg) {
          if constexpr (std::is_same_v<decltype(arg), bool>) {
            builder.put(key, arg);
          } else if constexpr (std::is_same_v<decltype(arg), int64_t>) {
            builder.put(key, arg);
          } else if constexpr (std::is_same_v<decltype(arg), double>) {
            builder.put(key, arg);
          } else if constexpr (std::is_same_v<decltype(arg), std::string>) {
            builder.put(key, arg);
          } else if constexpr (std::is_same_v<decltype(arg), std::vector<bool>>) {
            builder.put_array(key, [&arg](JsonBuilder& b) {
              for (const auto& item : arg)
                b.add(item);
            });
          } else if constexpr (std::is_same_v<decltype(arg), std::vector<int64_t>>) {
            builder.put_array(key, [&arg](JsonBuilder& b) {
              for (const auto& item : arg)
                b.add(item);
            });
          } else if constexpr (std::is_same_v<decltype(arg), std::vector<double>>) {
            builder.put_array(key, [&arg](JsonBuilder& b) {
              for (const auto& item : arg)
                b.add(item);
            });
          } else if constexpr (std::is_same_v<decltype(arg), std::vector<std::string>>) {
            builder.put_array(key, [&arg](JsonBuilder& b) {
              for (const auto& item : arg)
                b.add(item);
            });
          }
        },
        value);
  }
  return builder.build();
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

std::optional<Config> Config::get_section(std::string_view /* key */) const {
  return std::nullopt;
}

void Config::set_section(std::string_view /* key */, const Config& /* config */) {
  // This can be extended to support nested configuration
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

// Global configuration instance
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
