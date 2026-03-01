#include "veloz/core/config_manager.h"

#include "veloz/core/json.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/filesystem.h> // kj::newDiskFilesystem for filesystem operations
#include <kj/memory.h>

namespace veloz::core {

using JsonBuilder = veloz::core::JsonBuilder;
using JsonValue = veloz::core::JsonValue;

// ============================================================================
// ConfigManager Implementation
// ============================================================================

bool ConfigManager::load_from_json(const kj::Path& file_path, bool reload) {
  // Use KJ exception handling pattern
  kj::Maybe<kj::Exception> maybeException = kj::runCatchingExceptions([&]() {
    auto doc = JsonDocument::parse_file(file_path);
    auto root = doc.root();

    {
      auto state = state_.lockExclusive();
      state->config_file = file_path.clone();
    }

    // Apply all values from JSON
    root.for_each_object(
        [this](kj::StringPtr key, const JsonValue& value) { apply_json_value(key, value); });

    if (reload) {
      trigger_hot_reload();
    }
  });

  KJ_IF_SOME(exception, maybeException) {
    KJ_LOG(WARNING, "Failed to load JSON config", exception.getDescription());
    return false;
  }
  return true;
}

bool ConfigManager::load_from_json_string(kj::StringPtr json_content, bool reload) {
  // Use KJ exception handling pattern
  kj::Maybe<kj::Exception> maybeException = kj::runCatchingExceptions([&]() {
    auto doc = JsonDocument::parse(json_content);
    auto root = doc.root();

    // Apply all values from JSON
    root.for_each_object(
        [this](kj::StringPtr key, const JsonValue& value) { apply_json_value(key, value); });

    if (reload) {
      trigger_hot_reload();
    }
  });

  KJ_IF_SOME(exception, maybeException) {
    KJ_LOG(WARNING, "Failed to parse JSON string", exception.getDescription());
    return false;
  }
  return true;
}

bool ConfigManager::load_from_yaml(const kj::Path& file_path, bool reload) {
  // Use KJ exception handling pattern
  bool success = false;
  kj::Maybe<kj::Exception> maybeException = kj::runCatchingExceptions([&]() {
    // Convert kj::Path to string for std::ifstream
    auto path_str = file_path.toString();
    std::ifstream ifs(path_str.cStr());
    if (!ifs.is_open()) {
      return;
    }

    kj::Vector<char> content;
    for (std::istreambuf_iterator<char> it(ifs), end; it != end; ++it) {
      content.add(*it);
    }
    if (content.size() == 0) {
      return;
    }
    content.add('\0');
    kj::String content_str(content.releaseAsArray());

    success = load_from_json_string(content_str, reload);
  });

  KJ_IF_SOME(exception, maybeException) {
    KJ_LOG(WARNING, "Failed to load YAML config", exception.getDescription());
    return false;
  }
  return success;
}

bool ConfigManager::save_to_json(const kj::Path& file_path) const {
  // Use KJ exception handling pattern
  bool success = false;
  kj::Maybe<kj::Exception> maybeException = kj::runCatchingExceptions([&]() {
    // Get filesystem and root directory reference
    auto fs = kj::newDiskFilesystem();

    // Create directory if it doesn't exist using KJ filesystem API
    // openSubdir with CREATE | CREATE_PARENT creates all necessary parent directories
    if (file_path.size() > 1) {
      auto parent = file_path.parent();
      if (parent.size() > 0) {
        KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                     fs->getCurrent().openSubdir(parent, kj::WriteMode::CREATE |
                                                             kj::WriteMode::CREATE_PARENT);
                   })) {
          // Directory creation failed, log but continue
          KJ_LOG(WARNING, "Failed to create parent directory", exception.getDescription());
        }
      }
    }

    // Use existing to_json() method
    kj::String json_str = to_json();

    // Convert kj::Path to string for std::ofstream (kept for file writing)
    auto path_str = file_path.toString();
    std::ofstream ofs(path_str.cStr());
    if (!ofs.is_open()) {
      return;
    }

    ofs << json_str.cStr();
    success = true;
  });

  KJ_IF_SOME(exception, maybeException) {
    KJ_LOG(ERROR, "Exception in save_to_json", exception.getDescription());
    return false;
  }
  return success;
}

kj::String ConfigManager::to_json() const {
  kj::Vector<kj::String> parts;
  kj::Function<void(const ConfigGroup*, kj::Vector<kj::String>&)> append_group_json =
      [&](const ConfigGroup* group, kj::Vector<kj::String>& out) {
        kj::Vector<kj::String> item_parts;
        for (auto* item : group->get_items()) {
          if (!item->is_set()) {
            continue;
          }
          auto escaped_key = json_utils::escape_string(item->key());
          item_parts.add(kj::str("\"", escaped_key.cStr(), "\":", item->to_json_string()));
        }

        for (auto* sub_group : group->get_groups()) {
          auto escaped_name = json_utils::escape_string(sub_group->name());
          kj::Vector<kj::String> sub_group_parts;
          append_group_json(sub_group, sub_group_parts);
          kj::String sub_group_json = kj::str("{", kj::strArray(sub_group_parts, ","), "}");
          item_parts.add(kj::str("\"", escaped_name.cStr(), "\":", sub_group_json));
        }

        for (auto& part : item_parts) {
          out.add(kj::mv(part));
        }
      };

  append_group_json(root_group_.get(), parts);
  return kj::str("{", kj::strArray(parts, ","), "}");
}

ConfigItemBase* ConfigManager::find_item(kj::StringPtr path) const {
  // Parse path (e.g., "group.subgroup.item" or just "item")
  kj::Vector<kj::String> parts_vec;
  auto copy = kj::str(path);
  char* p = copy.begin();
  char* start = p;
  while (*p != '\0') {
    if (*p == '.') {
      parts_vec.add(kj::str(kj::ArrayPtr<const char>(start, p)));
      start = p + 1;
    }
    p++;
  }
  if (start < p) {
    parts_vec.add(kj::str(kj::ArrayPtr<const char>(start, p)));
  }

  if (parts_vec.size() == 0) {
    return nullptr;
  }

  if (parts_vec.size() == 1) {
    // Direct item lookup in root group
    for (auto* item : root_group_->get_items()) {
      if (item->key() == parts_vec[0]) {
        return item;
      }
    }
    return nullptr;
  }

  // Navigate through groups (use const version since this is a const method)
  const ConfigGroup* current = root_group_->get_group(parts_vec[0]);
  if (!current) {
    return nullptr;
  }

  for (size_t i = 1; i < parts_vec.size() - 1; ++i) {
    current = current->get_group(parts_vec[i]);
    if (!current) {
      return nullptr;
    }
  }

  // Find item in final group
  for (auto* item : current->get_items()) {
    if (item->key() == parts_vec.back()) {
      return item;
    }
  }

  return nullptr;
}

bool ConfigManager::validate() const {
  return root_group_->validate();
}

kj::Vector<kj::String> ConfigManager::validation_errors() const {
  return root_group_->validation_errors();
}

void ConfigManager::add_hot_reload_callback(HotReloadCallback callback) {
  state_.lockExclusive()->hot_reload_callbacks.add(kj::mv(callback));
}

void ConfigManager::trigger_hot_reload() {
  auto state = state_.lockExclusive();
  for (auto& callback : state->hot_reload_callbacks) {
    // Use KJ exception handling - swallow exceptions from callbacks
    KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() { callback(); })) {
      KJ_LOG(WARNING, "Hot reload callback failed", exception.getDescription());
    }
  }
}

void ConfigManager::set_config_file(const kj::Path& file_path) {
  state_.lockExclusive()->config_file = file_path.clone();
}

void ConfigManager::apply_json_value(kj::StringPtr key, const JsonValue& value) {
  if (value.is_object()) {
    value.for_each_object([this, key](kj::StringPtr child_key, const JsonValue& child_value) {
      if (key.size() == 0) {
        auto joined = kj::str(child_key);
        apply_json_value(joined, child_value);
      } else {
        auto joined = kj::str(key, ".", child_key);
        apply_json_value(joined, child_value);
      }
    });
    return;
  }

  auto find_item_in_group = [](ConfigGroup* group, kj::StringPtr item_key) -> ConfigItemBase* {
    for (auto* item : group->get_items()) {
      if (item->key() == item_key) {
        return item;
      }
    }
    return nullptr;
  };

  auto ensure_group_unlocked = [](ConfigGroup* parent, kj::StringPtr name) -> ConfigGroup* {
    if (auto* existing = parent->get_group(name)) {
      return existing;
    }
    auto new_group = kj::heap<ConfigGroup>(name);
    auto* raw = new_group.get();
    parent->add_group(kj::mv(new_group));
    return raw;
  };

  kj::Vector<kj::String> parts;
  size_t start = 0;
  for (;;) {
    KJ_IF_SOME(dot, key.slice(start).findFirst('.')) {
      size_t idx = start + dot;
      parts.add(kj::str(key.slice(start, idx)));
      start = idx + 1;
    }
    else {
      parts.add(kj::str(key.slice(start)));
      break;
    }
  }

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
    // Parse JSON value and convert to string
    kj::String str_val;

    if (value.is_bool()) {
      str_val = value.get_bool() ? kj::str("true"_kj) : kj::str("false"_kj);
    } else if (value.is_int()) {
      str_val = kj::str(value.get_int());
    } else if (value.is_real()) {
      str_val = kj::str(value.get_double());
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
          KJ_IF_SOME(sp, arr_val.get_string_ptr()) {
            arr_builder.add(sp);
          }
        }
      });
      str_val = arr_builder.build();
    }

    item->from_string(str_val);
    return;
  }

  kj::Own<ConfigItemBase> created;
  kj::StringPtr key_str = parts.back();
  if (value.is_bool()) {
    created = ConfigItem<bool>::Builder(key_str, ""_kj).build();
  } else if (value.is_int()) {
    created = ConfigItem<int>::Builder(key_str, ""_kj).build();
  } else if (value.is_real()) {
    created = ConfigItem<double>::Builder(key_str, ""_kj).build();
  } else if (value.is_string()) {
    created = ConfigItem<kj::String>::Builder(key_str, ""_kj).build();
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
      created = ConfigItem<kj::Vector<kj::String>>::Builder(key_str, ""_kj).build();
    } else if (seen_real) {
      created = ConfigItem<kj::Vector<double>>::Builder(key_str, ""_kj).build();
    } else if (seen_int) {
      created = ConfigItem<kj::Vector<int>>::Builder(key_str, ""_kj).build();
    } else if (seen_bool) {
      created = ConfigItem<kj::Vector<bool>>::Builder(key_str, ""_kj).build();
    } else {
      created = ConfigItem<kj::Vector<kj::String>>::Builder(key_str, ""_kj).build();
    }
  }

  if (!created) {
    return;
  }

  kj::String str_val;
  if (value.is_bool()) {
    str_val = value.get_bool() ? kj::str("true"_kj) : kj::str("false"_kj);
  } else if (value.is_int()) {
    str_val = kj::str(value.get_int());
  } else if (value.is_real()) {
    str_val = kj::str(value.get_double());
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
        KJ_IF_SOME(sp, arr_val.get_string_ptr()) {
          arr_builder.add(sp);
        }
      }
    });
    str_val = arr_builder.build();
  }

  // Set the value BEFORE transferring ownership to avoid dangling pointer
  created->from_string(str_val);
  current_group->add_item(kj::mv(created));
}

} // namespace veloz::core
