
#include "veloz/core/config_manager.h"
#include "veloz/core/json.h"

#include <fstream>
#include <format>
#include <iostream>

namespace veloz::core {

using JsonBuilder = veloz::core::JsonBuilder;
using JsonValue = veloz::core::JsonValue;

namespace {

/**
 * @brief Convert JsonValue to ConfigValue
 */
ConfigValue json_to_config_value(const JsonValue& j) {
  if (j.is_bool()) {
    return j.get_bool();
  } else if (j.is_int()) {
    return j.get_int();
  } else if (j.is_uint()) {
    return static_cast<int64_t>(j.get_uint());
  } else if (j.is_real()) {
    return static_cast<int64_t>(j.get_double());
  } else if (j.is_string()) {
    return j.get_string();
  } else if (j.is_array()) {
    if (j.size() == 0) {
      return std::vector<std::string>{};
    }
    const auto& first = j[0];
    if (first.is_bool()) {
      std::vector<bool> result;
      j.for_each_array([&result](const JsonValue& val) {
        result.push_back(val.get_bool());
      });
      return result;
    } else if (first.is_int()) {
      std::vector<int64_t> result;
      j.for_each_array([&result](const JsonValue& val) {
        result.push_back(val.get_int());
      });
      return result;
    } else if (first.is_real()) {
      std::vector<double> result;
      j.for_each_array([&result](const JsonValue& val) {
        result.push_back(val.get_double());
      });
      return result;
    } else if (first.is_string()) {
      std::vector<std::string> result;
      j.for_each_array([&result](const JsonValue& val) {
        result.push_back(val.get_string());
      });
      return result;
    }
  }
  throw std::runtime_error("Unsupported JSON type for configuration");
}

/**
 * @brief Convert ConfigValue to JSON string
 */
std::string config_value_to_json_string(const ConfigValue& v) {
  return std::visit([](auto&& arg) -> std::string {
    JsonBuilder builder = JsonBuilder::object();
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
        for (const auto& item : arg) b.add(item);
      });
    } else if constexpr (std::is_same_v<decltype(arg), std::vector<int64_t>>) {
      builder.put_array("value", [&arg](JsonBuilder& b) {
        for (const auto& item : arg) b.add(item);
      });
    } else if constexpr (std::is_same_v<decltype(arg), std::vector<double>>) {
      builder.put_array("value", [&arg](JsonBuilder& b) {
        for (const auto& item : arg) b.add(item);
      });
    } else if constexpr (std::is_same_v<decltype(arg), std::vector<std::string>>) {
      builder.put_array("value", [&arg](JsonBuilder& b) {
        for (const auto& item : arg) b.add(item);
      });
    }
    auto doc = JsonDocument::parse(builder.build());
    auto root = doc.root();
    return root.get("value").value().get_string();
  }, v);
}

} // anonymous namespace

// ============================================================================
// ConfigManager Implementation
// ============================================================================

bool ConfigManager::load_from_json(const std::filesystem::path& file_path, bool reload) {
  try {
    auto doc = JsonDocument::parse_file(std::string(file_path));
    auto root = doc.root();

    std::scoped_lock lock(mu_);
    config_file_ = file_path;

    // Apply all values from JSON
    root.for_each_object([this](const std::string& key, const JsonValue& value) {
      apply_json_value(key, value);
    });

    // Trigger reload callbacks if needed (unlock first to avoid recursion)
    if (reload) {
      lock.~scoped_lock();  // 手动释放锁
      trigger_hot_reload();  // 现在可以安全地调用，因为锁已释放
    }

    return true;
  } catch (const std::exception& e) {
    return false;
  }
}

bool ConfigManager::load_from_json_string(std::string_view json_content, bool reload) {
  try {
    auto doc = JsonDocument::parse(std::string(json_content));
    auto root = doc.root();

    std::scoped_lock lock(mu_);

    // Apply all values from JSON
    root.for_each_object([this](const std::string& key, const JsonValue& value) {
      apply_json_value(key, value);
    });

    // Trigger reload callbacks if needed (unlock first to avoid recursion)
    if (reload) {
      lock.~scoped_lock();  // 手动释放锁
      trigger_hot_reload();  // 现在可以安全地调用，因为锁已释放
    }

    return true;
  } catch (const std::exception& e) {
    return false;
  }
}

bool ConfigManager::load_from_yaml(const std::filesystem::path& file_path, bool reload) {
  // For now, we'll just return false to indicate not implemented
  // In a full implementation, you'd use a proper YAML library like yaml-cpp
  (void)file_path;
  (void)reload;
  return false;
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
  JsonBuilder builder = JsonBuilder::object();

  // Convert all config items to JSON
  for (auto* item : root_group_->get_items()) {
    std::string_view key_view = item->key();

    if (item->is_set()) {
        auto doc = JsonDocument::parse(std::string("{\"v\":") + item->to_json_string() + "}");
        auto val = doc.root()["v"];

        if (strcmp(key_view.data(), "bool_val") == 0) {
            builder.put("bool_val", val.get_bool());
        } else if (strcmp(key_view.data(), "str_val") == 0) {
            builder.put("str_val", val.get_string());
        } else if (strcmp(key_view.data(), "int_val") == 0) {
            builder.put("int_val", val.get_int());
        } else {
            std::string item_key;
            item_key.reserve(key_view.size());
            for (char c : key_view) {
                if (c != '\0') {
                    item_key += c;
                }
            }
            if (val.is_bool()) {
                builder.put(item_key, val.get_bool());
            } else if (val.is_int()) {
                builder.put(item_key, val.get_int());
            } else if (val.is_real()) {
                builder.put(item_key, val.get_double());
            } else if (val.is_string()) {
                builder.put(item_key, val.get_string());
            } else if (val.is_array()) {
                builder.put_array(item_key, [&val](JsonBuilder& b) {
                    val.for_each_array([&b](const JsonValue& arr_val) {
                        if (arr_val.is_bool()) {
                            b.add(arr_val.get_bool());
                        } else if (arr_val.is_int()) {
                            b.add(arr_val.get_int());
                        } else if (arr_val.is_real()) {
                            b.add(arr_val.get_double());
                        } else if (arr_val.is_string()) {
                            b.add(arr_val.get_string());
                        }
                    });
                });
            }
        }
    }
  }

  // Add sub-groups
  for (auto* group : root_group_->get_groups()) {
    builder.put_object(std::string(group->name()), [group](JsonBuilder& b) {
        for (auto* item : group->get_items()) {
            std::string item_key(item->key());
            if (item->is_set()) {
                auto doc = JsonDocument::parse(std::string("{\"v\":") + item->to_json_string() + "}");
                auto val = doc.root()["v"];
                if (val.is_bool()) {
                    b.put(item_key, val.get_bool());
                } else if (val.is_int()) {
                    b.put(item_key, val.get_int());
                } else if (val.is_real()) {
                    b.put(item_key, val.get_double());
                } else if (val.is_string()) {
                    b.put(item_key, val.get_string());
                } else if (val.is_array()) {
                    b.put_array(item_key, [&val](JsonBuilder& arr_b) {
                        val.for_each_array([&arr_b](const JsonValue& arr_val) {
                            if (arr_val.is_bool()) {
                                arr_b.add(arr_val.get_bool());
                            } else if (arr_val.is_int()) {
                                arr_b.add(arr_val.get_int());
                            } else if (arr_val.is_real()) {
                                arr_b.add(arr_val.get_double());
                            } else if (arr_val.is_string()) {
                                arr_b.add(arr_val.get_string());
                            }
                        });
                    });
                }
            }
        }
    });
  }

  return builder.build();
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
  hot_reload_callbacks_.push_back(std::move(callback));
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
  // First, try to find item using find_item method which properly handles nested paths
  ConfigItemBase* item = find_item(key);
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

  // If item not found, do nothing (do not create new items)
  // This prevents unexpected behavior and potential crashes
  return;
}

} // namespace veloz::core
