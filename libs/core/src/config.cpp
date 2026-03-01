#include "veloz/core/config.h"

#include "veloz/core/json.h"

#include <fstream>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/exception.h>
#include <kj/filesystem.h>
#include <kj/memory.h>
#include <kj/mutex.h>

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
      return kj::heapArray<kj::String>(0);
    }
    const auto& first = j[0];
    if (first.is_bool()) {
      auto builder = kj::heapArrayBuilder<bool>(j.size());
      j.for_each_array([&builder](const JsonValue& val) { builder.add(val.get_bool()); });
      return builder.finish();
    } else if (first.is_int()) {
      auto builder = kj::heapArrayBuilder<int64_t>(j.size());
      j.for_each_array([&builder](const JsonValue& val) { builder.add(val.get_int()); });
      return builder.finish();
    } else if (first.is_real()) {
      auto builder = kj::heapArrayBuilder<double>(j.size());
      j.for_each_array([&builder](const JsonValue& val) { builder.add(val.get_double()); });
      return builder.finish();
    } else if (first.is_string()) {
      auto builder = kj::heapArrayBuilder<kj::String>(j.size());
      j.for_each_array([&builder](const JsonValue& val) { builder.add(val.get_string()); });
      return builder.finish();
    }
  }
  KJ_FAIL_REQUIRE("Unsupported JSON type");
}

Config::Value clone_value(const Config::Value& v) {
  KJ_SWITCH_ONEOF(v) {
    KJ_CASE_ONEOF(b, bool) {
      return b;
    }
    KJ_CASE_ONEOF(i, int64_t) {
      return i;
    }
    KJ_CASE_ONEOF(d, double) {
      return d;
    }
    KJ_CASE_ONEOF(s, kj::String) {
      return kj::heapString(s.cStr(), s.size());
    }
    KJ_CASE_ONEOF(a, kj::Array<bool>) {
      auto builder = kj::heapArrayBuilder<bool>(a.size());
      for (auto item : a) {
        builder.add(item);
      }
      return builder.finish();
    }
    KJ_CASE_ONEOF(a, kj::Array<int64_t>) {
      auto builder = kj::heapArrayBuilder<int64_t>(a.size());
      for (auto item : a) {
        builder.add(item);
      }
      return builder.finish();
    }
    KJ_CASE_ONEOF(a, kj::Array<double>) {
      auto builder = kj::heapArrayBuilder<double>(a.size());
      for (auto item : a) {
        builder.add(item);
      }
      return builder.finish();
    }
    KJ_CASE_ONEOF(a, kj::Array<kj::String>) {
      auto builder = kj::heapArrayBuilder<kj::String>(a.size());
      for (const auto& item : a) {
        builder.add(kj::heapString(item.cStr(), item.size()));
      }
      return builder.finish();
    }
  }
  KJ_UNREACHABLE;
}

} // namespace

Config::Config(const kj::Path& file_path) {
  load_from_file(file_path);
}

Config::Config(kj::StringPtr json_content) {
  load_from_string(json_content);
}

bool Config::load_from_file(const kj::Path& file_path) {
  bool success = false;
  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               auto path_str = file_path.toString();
               auto doc = JsonDocument::parse_file(file_path);
               auto root = doc.root();

               config_.clear();
               root.for_each_object([this](kj::StringPtr key, const JsonValue& value) {
                 config_.upsert(kj::str(key), json_to_value(value));
               });
               success = true;
             })) {
    (void)exception;
    return false;
  }
  return success;
}

bool Config::load_from_string(kj::StringPtr json_content) {
  bool success = false;
  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               auto doc = JsonDocument::parse(json_content);
               auto root = doc.root();

               config_.clear();
               root.for_each_object([this](kj::StringPtr key, const JsonValue& value) {
                 config_.upsert(kj::str(key), json_to_value(value));
               });
               success = true;
             })) {
    (void)exception;
    return false;
  }
  return success;
}

bool Config::save_to_file(const kj::Path& file_path) const {
  bool success = false;
  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               auto path_str = file_path.toString();
               std::ofstream ofs(path_str.cStr());
               KJ_REQUIRE(ofs.is_open(), "Failed to open file for writing", path_str.cStr());
               auto s = to_string();
               ofs << s.cStr();
               success = true;
             })) {
    (void)exception;
    return false;
  }
  return success;
}

kj::String Config::to_string() const {
  auto builder = JsonBuilder::object();
  for (const auto& entry : config_) {
    kj::StringPtr key = entry.key;
    const auto& value = entry.value;
    KJ_SWITCH_ONEOF(value) {
      KJ_CASE_ONEOF(b, bool) {
        builder.put(key, b);
      }
      KJ_CASE_ONEOF(i, int64_t) {
        builder.put(key, i);
      }
      KJ_CASE_ONEOF(d, double) {
        builder.put(key, d);
      }
      KJ_CASE_ONEOF(s, kj::String) {
        builder.put(key, s);
      }
      KJ_CASE_ONEOF(a, kj::Array<bool>) {
        builder.put_array(key, [&a](JsonBuilder& b) {
          for (auto item : a) {
            b.add(item);
          }
        });
      }
      KJ_CASE_ONEOF(a, kj::Array<int64_t>) {
        builder.put_array(key, [&a](JsonBuilder& b) {
          for (auto item : a) {
            b.add(item);
          }
        });
      }
      KJ_CASE_ONEOF(a, kj::Array<double>) {
        builder.put_array(key, [&a](JsonBuilder& b) {
          for (auto item : a) {
            b.add(item);
          }
        });
      }
      KJ_CASE_ONEOF(a, kj::Array<kj::String>) {
        builder.put_array(key, [&a](JsonBuilder& b) {
          for (const auto& item : a) {
            b.add(item);
          }
        });
      }
    }
  }
  return builder.build();
}

bool Config::has_key(kj::StringPtr key) const {
  return config_.find(key) != kj::none;
}

void Config::set(kj::StringPtr key, Value value) {
  config_.upsert(kj::str(key), kj::mv(value));
}

void Config::remove(kj::StringPtr key) {
  config_.erase(key);
}

kj::Maybe<Config> Config::get_section(kj::StringPtr /* key */) const {
  return kj::none;
}

void Config::set_section(kj::StringPtr /* key */, const Config& /* config */) {}

void Config::merge(const Config& other) {
  for (const auto& entry : other.config_) {
    config_.upsert(kj::heapString(entry.key.cStr(), entry.key.size()), clone_value(entry.value));
  }
}

kj::Array<kj::String> Config::keys() const {
  auto builder = kj::heapArrayBuilder<kj::String>(config_.size());
  for (const auto& entry : config_) {
    builder.add(kj::heapString(entry.key.cStr(), entry.key.size()));
  }
  return builder.finish();
}

// Thread-safe global configuration using KJ MutexGuarded (no bare pointers)
struct GlobalConfigState {
  kj::Maybe<kj::Own<Config>> config{kj::none};
};
static kj::MutexGuarded<GlobalConfigState> g_global_config;

Config& global_config() {
  auto lock = g_global_config.lockExclusive();
  KJ_IF_SOME(config, lock->config) {
    return *config; // Dereference kj::Own<Config> to get Config reference
  }
  // Create new config and store it
  auto newConfig = kj::heap<Config>();
  Config& ref = *newConfig;
  lock->config = kj::mv(newConfig);
  return ref;
}

bool load_global_config(const kj::Path& file_path) {
  return global_config().load_from_file(file_path);
}

bool load_global_config(kj::StringPtr json_content) {
  return global_config().load_from_string(json_content);
}

} // namespace veloz::core
