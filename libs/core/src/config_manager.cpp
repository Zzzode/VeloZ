
#include "veloz/core/config_manager.h"

#include "veloz/core/json.h"

#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <kj/common.h>
#include <kj/memory.h>

namespace veloz::core {

using JsonBuilder = veloz::core::JsonBuilder;
using JsonValue = veloz::core::JsonValue;

// ============================================================================
// ConfigManager Implementation
// ============================================================================

bool ConfigManager::load_from_json(const std::filesystem::path& file_path, bool reload) {
  try {
    auto doc = JsonDocument::parse_file(std::string(file_path));
    auto root = doc.root();

    {
      std::scoped_lock lock(mu_);
      config_file_ = file_path;
    }

    // Apply all values from JSON
    root.for_each_object(
        [this](const std::string& key, const JsonValue& value) { apply_json_value(key, value); });

    if (reload) {
      trigger_hot_reload();
    }

    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool ConfigManager::load_from_json_string(std::string_view json_content, bool reload) {
  try {
    auto doc = JsonDocument::parse(std::string(json_content));
    auto root = doc.root();

    // Apply all values from JSON
    root.for_each_object(
        [this](const std::string& key, const JsonValue& value) { apply_json_value(key, value); });

    if (reload) {
      trigger_hot_reload();
    }

    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool ConfigManager::load_from_yaml(const std::filesystem::path& file_path, bool reload) {
  try {
    std::ifstream ifs(file_path);
    if (!ifs.is_open()) {
      return false;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (content.empty()) {
      return false;
    }

    return load_from_json_string(content, reload);
  } catch (const std::exception&) {
    return false;
  }
}

bool ConfigManager::save_to_json(const std::filesystem::path& file_path) const {
  try {
    // Create directory if it doesn't exist
    if (auto parent_path = file_path.parent_path(); !parent_path.empty()) {
      std::filesystem::create_directories(parent_path);
    }

    // Use the existing to_json() method which is correct
    std::string json_str = to_json();

    std::ofstream ofs(file_path);
    if (!ofs.is_open()) {
      return false;
    }

    ofs << json_str;
    return true;
  } catch (const std::exception& e) {
    std::cerr << "Exception in save_to_json: " << e.what() << std::endl;
    return false;
  }
}

std::string ConfigManager::to_json() const {
  std::function<void(const ConfigGroup*, std::string&)> append_group_json =
      [&](const ConfigGroup* group, std::string& out) {
        out += "{";
        bool first = true;

        for (auto* item : group->get_items()) {
          if (!item->is_set()) {
            continue;
          }
          if (!first) {
            out += ",";
          }
          std::string key = json_utils::escape_string(item->key());
          out += "\"";
          out += key;
          out += "\":";
          out += item->to_json_string();
          first = false;
        }

        for (auto* sub_group : group->get_groups()) {
          if (!first) {
            out += ",";
          }
          std::string key = json_utils::escape_string(sub_group->name());
          out += "\"";
          out += key;
          out += "\":";
          append_group_json(sub_group, out);
          first = false;
        }

        out += "}";
      };

  std::string result;
  append_group_json(root_group_.get(), result);
  return result;
}

ConfigItemBase* ConfigManager::find_item(std::string_view path) const {
  std::scoped_lock lock(mu_);

  // Parse path (e.g., "group.subgroup.item" or just "item")
  std::string p = std::string(path);
  std::vector<std::string> parts;
  size_t pos = 0;
  while ((pos = p.find('.')) != std::string::npos) {
    parts.push_back(p.substr(0, pos));
    p.erase(0, pos + 1);
  }
  parts.push_back(p);

  if (parts.empty()) {
    return nullptr;
  }

  if (parts.size() == 1) {
    // Direct item lookup in root group
    for (auto* item : root_group_->get_items()) {
      std::string item_key(item->key());
      if (item_key == parts[0]) {
        return item;
      }
    }
    return nullptr;
  }

  // Navigate through groups
  ConfigGroup* current = root_group_->get_group(parts[0]);
  if (!current) {
    return nullptr;
  }

  for (size_t i = 1; i < parts.size() - 1; ++i) {
    current = current->get_group(parts[i]);
    if (!current) {
      return nullptr;
    }
  }

  // Find item in final group
  for (auto* item : current->get_items()) {
    std::string item_key(item->key());
    if (item_key == parts.back()) {
      return item;
    }
  }

  return nullptr;
}

bool ConfigManager::validate() const {
  return root_group_->validate();
}

std::vector<std::string> ConfigManager::validation_errors() const {
  return root_group_->validation_errors();
}

void ConfigManager::add_hot_reload_callback(HotReloadCallback callback) {
  std::scoped_lock lock(mu_);
  hot_reload_callbacks_.push_back(kj::mv(callback));
}

void ConfigManager::trigger_hot_reload() {
  std::scoped_lock lock(mu_);
  for (const auto& callback : hot_reload_callbacks_) {
    try {
      callback();
    } catch (...) {
      // Swallow exceptions from callbacks
    }
  }
}

void ConfigManager::set_config_file(const std::filesystem::path& file_path) {
  std::scoped_lock lock(mu_);
  config_file_ = file_path;
}

void ConfigManager::apply_json_value(const std::string& key, const JsonValue& value) {
  if (value.is_object()) {
    value.for_each_object([this, &key](const std::string& child_key, const JsonValue& child_value) {
      if (key.empty()) {
        apply_json_value(child_key, child_value);
      } else {
        apply_json_value(key + "." + child_key, child_value);
      }
    });
    return;
  }

  auto find_item_in_group = [](ConfigGroup* group, std::string_view item_key) -> ConfigItemBase* {
    for (auto* item : group->get_items()) {
      if (item->key() == item_key) {
        return item;
      }
    }
    return nullptr;
  };

  auto ensure_group_unlocked = [](ConfigGroup* parent, std::string_view name) -> ConfigGroup* {
    if (auto* existing = parent->get_group(name)) {
      return existing;
    }
    auto new_group = std::make_unique<ConfigGroup>(std::string(name));
    auto* raw = new_group.get();
    parent->add_group(kj::mv(new_group));
    return raw;
  };

  std::scoped_lock lock(mu_);

  std::string p = key;
  std::vector<std::string> parts;
  size_t pos = 0;
  while ((pos = p.find('.')) != std::string::npos) {
    parts.push_back(p.substr(0, pos));
    p.erase(0, pos + 1);
  }
  parts.push_back(p);

  if (parts.empty()) {
    return;
  }

  ConfigGroup* current_group = root_group_.get();
  if (parts.size() > 1) {
    for (size_t i = 0; i < parts.size() - 1; ++i) {
      current_group = ensure_group_unlocked(current_group, parts[i]);
      if (!current_group) {
        return;
      }
    }
  }

  ConfigItemBase* item = find_item_in_group(current_group, parts.back());
  if (item) {
    // Parse the JSON value and convert to string
    std::string str_val;

    if (value.is_bool()) {
      str_val = value.get_bool() ? "true" : "false";
    } else if (value.is_int()) {
      str_val = std::to_string(value.get_int());
    } else if (value.is_real()) {
      str_val = std::to_string(value.get_double());
    } else if (value.is_string()) {
      str_val = value.get_string();
    } else if (value.is_array()) {
      // For arrays, we need to serialize to JSON
      JsonBuilder arr_builder = JsonBuilder::array();
      value.for_each_array([&arr_builder](const JsonValue& arr_val) {
        if (arr_val.is_bool()) {
          arr_builder.add(arr_val.get_bool());
        } else if (arr_val.is_int()) {
          arr_builder.add(arr_val.get_int());
        } else if (arr_val.is_real()) {
          arr_builder.add(arr_val.get_double());
        } else if (arr_val.is_string()) {
          arr_builder.add(arr_val.get_string());
        }
      });
      str_val = arr_builder.build();
    }

    item->from_string(str_val);
    return;
  }

  std::unique_ptr<ConfigItemBase> created;
  if (value.is_bool()) {
    created = ConfigItem<bool>::Builder(parts.back(), "").build();
  } else if (value.is_int()) {
    created = ConfigItem<int>::Builder(parts.back(), "").build();
  } else if (value.is_real()) {
    created = ConfigItem<double>::Builder(parts.back(), "").build();
  } else if (value.is_string()) {
    created = ConfigItem<std::string>::Builder(parts.back(), "").build();
  } else if (value.is_array()) {
    bool seen_string = false;
    bool seen_real = false;
    bool seen_int = false;
    bool seen_bool = false;
    value.for_each_array([&](const JsonValue& arr_val) {
      if (arr_val.is_string()) {
        seen_string = true;
      } else if (arr_val.is_real()) {
        seen_real = true;
      } else if (arr_val.is_int()) {
        seen_int = true;
      } else if (arr_val.is_bool()) {
        seen_bool = true;
      }
    });

    if (seen_string) {
      created = ConfigItem<std::vector<std::string>>::Builder(parts.back(), "").build();
    } else if (seen_real) {
      created = ConfigItem<std::vector<double>>::Builder(parts.back(), "").build();
    } else if (seen_int) {
      created = ConfigItem<std::vector<int>>::Builder(parts.back(), "").build();
    } else if (seen_bool) {
      created = ConfigItem<std::vector<bool>>::Builder(parts.back(), "").build();
    } else {
      created = ConfigItem<std::vector<std::string>>::Builder(parts.back(), "").build();
    }
  }

  if (!created) {
    return;
  }

  std::string str_val;
  if (value.is_bool()) {
    str_val = value.get_bool() ? "true" : "false";
  } else if (value.is_int()) {
    str_val = std::to_string(value.get_int());
  } else if (value.is_real()) {
    str_val = std::to_string(value.get_double());
  } else if (value.is_string()) {
    str_val = value.get_string();
  } else if (value.is_array()) {
    JsonBuilder arr_builder = JsonBuilder::array();
    value.for_each_array([&arr_builder](const JsonValue& arr_val) {
      if (arr_val.is_bool()) {
        arr_builder.add(arr_val.get_bool());
      } else if (arr_val.is_int()) {
        arr_builder.add(arr_val.get_int());
      } else if (arr_val.is_real()) {
        arr_builder.add(arr_val.get_double());
      } else if (arr_val.is_string()) {
        arr_builder.add(arr_val.get_string());
      }
    });
    str_val = arr_builder.build();
  }

  ConfigItemBase* created_raw = created.get();
  current_group->add_item(kj::mv(created));
  created_raw->from_string(str_val);
}

} // namespace veloz::core
