#include "handlers/config_handler.h"

#include "audit/audit_logger.h"
#include "auth/rbac.h"
#include "veloz/core/json.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <kj/debug.h>
#include <kj/time.h>

namespace veloz::gateway {

namespace {

// Simple JSON escaping for output
kj::String escapeJson(kj::StringPtr str) {
  kj::Vector<char> result;
  for (char c : str) {
    switch (c) {
    case '"':
      result.add('\\');
      result.add('"');
      break;
    case '\\':
      result.add('\\');
      result.add('\\');
      break;
    case '\n':
      result.add('\\');
      result.add('n');
      break;
    case '\r':
      result.add('\\');
      result.add('r');
      break;
    case '\t':
      result.add('\\');
      result.add('t');
      break;
    default:
      result.add(c);
      break;
    }
  }
  result.add('\0');
  return kj::String(result.releaseAsArray());
}

// Get current timestamp in ISO8601 format (currently unused but kept for future use)
[[maybe_unused]] kj::String getCurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time));
  return kj::heapString(buffer);
}

} // namespace

ConfigHandler::ConfigHandler(audit::AuditLogger& audit) : audit_(audit) {
  initialize_defaults();
}

kj::Promise<void> ConfigHandler::handleGetConfig(RequestContext& ctx) {
  // Permission check - read access allowed for all authenticated users
  if (!checkPermission(ctx, auth::Permission::ReadConfig)) {
    return ctx.sendError(403, "Permission denied: read:config required");
  }

  try {
    auto state = state_.lockExclusive();
    auto builder = core::JsonBuilder::object();

    builder.put("status", "success");
    builder.put_object("data", [&state](core::JsonBuilder& data) {
      for (auto& entry : state->config_values) {
        const auto& value = entry.value;
        switch (value.type) {
        case ConfigValue::Type::String:
          KJ_IF_SOME(str, value.data.tryGet<ConfigValue::StringData>()) {
            data.put(entry.key.cStr(), str.value.cStr());
          }
          break;
        case ConfigValue::Type::Number:
          KJ_IF_SOME(num, value.data.tryGet<ConfigValue::NumberData>()) {
            data.put(entry.key.cStr(), num.value);
          }
          break;
        case ConfigValue::Type::Boolean:
          KJ_IF_SOME(flag, value.data.tryGet<ConfigValue::BooleanData>()) {
            data.put(entry.key.cStr(), flag.value);
          }
          break;
        default:
          break;
        }
      }
    });

    kj::String json_body = builder.build();

    // Log audit event (fire-and-forget, queue is handled by background thread)
    kj::String userId = kj::str("unknown");
    KJ_IF_SOME(auth, ctx.authInfo) {
      userId = kj::heapString(auth.user_id);
    }
    (void)audit_.log(audit::AuditLogType::Access, kj::str("CONFIG_QUERY"), kj::mv(userId),
                     kj::heapString(ctx.clientIP), kj::none);

    return ctx.sendJson(200, json_body);
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Error in config handler", e);
    return ctx.sendError(500, "Internal Server Error"_kj);
  } catch (...) {
    KJ_LOG(ERROR, "Unknown error in config handler");
    return ctx.sendError(500, "Internal Server Error"_kj);
  }
}

kj::Promise<void> ConfigHandler::handleGetConfigKey(RequestContext& ctx) {
  // Permission check
  if (!checkPermission(ctx, auth::Permission::ReadConfig)) {
    return ctx.sendError(403, "Permission denied: read:config required");
  }

  try {
    // Extract key from path parameters
    KJ_IF_SOME(key, ctx.path_params.find("key"_kj)) {
      auto state = state_.lockExclusive();

      KJ_IF_SOME(value_ptr, state->config_values.find(key)) {
        auto builder = core::JsonBuilder::object();
        builder.put("status", "success");
        builder.put("key", key.cStr());

        const auto& value = value_ptr;
        switch (value.type) {
        case ConfigValue::Type::String:
          KJ_IF_SOME(str, value.data.tryGet<ConfigValue::StringData>()) {
            builder.put("value", str.value.cStr());
          }
          break;
        case ConfigValue::Type::Number:
          KJ_IF_SOME(num, value.data.tryGet<ConfigValue::NumberData>()) {
            builder.put("value", num.value);
          }
          break;
        case ConfigValue::Type::Boolean:
          KJ_IF_SOME(flag, value.data.tryGet<ConfigValue::BooleanData>()) {
            builder.put("value", flag.value);
          }
          break;
        default:
          break;
        }

        kj::String json_body = builder.build();
        return ctx.sendJson(200, json_body);
      }
      else {
        return ctx.sendError(404, "Configuration key not found");
      }
    }
    else {
      return ctx.sendError(400, "Missing key parameter");
    }
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Error in config handler", e);
    return ctx.sendError(500, "Internal Server Error"_kj);
  } catch (...) {
    KJ_LOG(ERROR, "Unknown error in config handler");
    return ctx.sendError(500, "Internal Server Error"_kj);
  }
}

kj::Promise<void> ConfigHandler::handleUpdateConfig(RequestContext& ctx) {
  // Permission check - requires admin access
  if (!checkPermission(ctx, auth::Permission::AdminConfig)) {
    return ctx.sendError(403, "Permission denied: admin:config required");
  }

  // Read request body
  return ctx.readBodyAsString().then([this, &ctx](kj::String body) -> kj::Promise<void> {
    try {
      // Parse JSON document
      auto doc = core::JsonDocument::parse(body);
      auto root = doc.root();

      if (!root.is_object()) {
        return ctx.sendError(400, "Invalid configuration: expected object");
      }

      auto state = state_.lockExclusive();
      size_t updated_count = 0;

      // Iterate through object properties
      root.for_each_object(
          [this, &state, &updated_count](kj::StringPtr key, const core::JsonValue& value) {
            // Check if key is read-only
            for (const auto& readonly_key : state->readonly_keys) {
              if (readonly_key == key) {
                return; // Skip read-only keys
              }
            }

            // Validate key
            kj::String error;
            if (!validateConfigKey(key, error)) {
              return; // Skip invalid keys
            }

            // Parse value and update config
            ConfigValue new_value;
            if (value.is_string()) {
              new_value = ConfigValue{value.get_string()};
            } else if (value.is_real() || value.is_int() || value.is_uint()) {
              new_value = ConfigValue{value.get_double()};
            } else if (value.is_bool()) {
              new_value = ConfigValue{value.get_bool()};
            } else {
              return; // Skip unsupported types
            }

            state->config_values.insert(kj::str(key), kj::mv(new_value));
            ++updated_count;
          });

      // Log audit event (fire-and-forget)
      kj::String userId = kj::str("unknown");
      KJ_IF_SOME(auth, ctx.authInfo) {
        userId = kj::heapString(auth.user_id);
      }
      (void)audit_.log(audit::AuditLogType::Access, kj::str("CONFIG_UPDATE"), kj::mv(userId),
                       kj::heapString(ctx.clientIP), kj::none);

      auto builder = core::JsonBuilder::object();
      builder.put("status", "success");
      builder.put("updated_count", static_cast<int64_t>(updated_count));
      builder.put("updated_at", getCurrentTimestamp().cStr());

      kj::String json_body = builder.build();
      return ctx.sendJson(200, json_body);
    } catch (const kj::Exception& e) {
      KJ_LOG(ERROR, "Error parsing config update", e);
      return ctx.sendError(400, "Invalid JSON in request body");
    } catch (...) {
      return ctx.sendError(400, "Invalid JSON in request body");
    }
  });
}

kj::Promise<void> ConfigHandler::handleUpdateConfigKey(RequestContext& ctx) {
  // Permission check
  if (!checkPermission(ctx, auth::Permission::AdminConfig)) {
    return ctx.sendError(403, "Permission denied: admin:config required");
  }

  // Extract key from path parameters
  KJ_IF_SOME(key, ctx.path_params.find("key"_kj)) {
    // Check if key is read-only
    auto state = state_.lockExclusive();
    for (const auto& readonly_key : state->readonly_keys) {
      if (readonly_key == key) {
        auto builder = core::JsonBuilder::object();
        builder.put("status", "error");
        builder.put("error", "Configuration key is read-only");
        kj::String json_body = builder.build();
        return ctx.sendJson(403, json_body);
      }
    }
  }
  else {
    return ctx.sendError(400, "Missing key parameter");
  }

  // Read request body and parse value
  return ctx.readBodyAsString().then([this, &ctx](kj::String body) -> kj::Promise<void> {
    try {
      auto doc = core::JsonDocument::parse(body);
      auto root = doc.root();

      KJ_IF_SOME(key, ctx.path_params.find("key"_kj)) {
        // Validate key
        kj::String error;
        if (!validateConfigKey(key, error)) {
          auto builder = core::JsonBuilder::object();
          builder.put("status", "error");
          builder.put("error", error.cStr());
          kj::String json_body = builder.build();
          return ctx.sendJson(400, json_body);
        }

        // Parse value
        ConfigValue new_value;
        if (root.is_string()) {
          new_value = ConfigValue{root.get_string()};
        } else if (root.is_real() || root.is_int() || root.is_uint()) {
          new_value = ConfigValue{root.get_double()};
        } else if (root.is_bool()) {
          new_value = ConfigValue{root.get_bool()};
        } else {
          return ctx.sendError(400, "Invalid value type: expected string, number, or boolean");
        }

        // Update configuration
        auto state = state_.lockExclusive();
        state->config_values.insert(kj::str(key), kj::mv(new_value));

        // Log audit event (fire-and-forget)
        kj::String userId = kj::str("unknown");
        KJ_IF_SOME(auth, ctx.authInfo) {
          userId = kj::heapString(auth.user_id);
        }
        (void)audit_.log(audit::AuditLogType::Access, kj::str("CONFIG_KEY_UPDATE"), kj::mv(userId),
                         kj::heapString(ctx.clientIP), kj::none);

        auto builder = core::JsonBuilder::object();
        builder.put("status", "success");
        builder.put("key", key.cStr());
        builder.put("updated_at", getCurrentTimestamp().cStr());

        kj::String json_body = builder.build();
        return ctx.sendJson(200, json_body);
      }
      // Key not found in path params (shouldn't happen due to earlier check)
      return ctx.sendError(400, "Missing key parameter");
    } catch (const kj::Exception& e) {
      KJ_LOG(ERROR, "Error parsing config value", e);
      return ctx.sendError(400, "Invalid JSON in request body");
    } catch (...) {
      return ctx.sendError(400, "Invalid JSON in request body");
    }
  });
}

kj::Promise<void> ConfigHandler::handleDeleteConfigKey(RequestContext& ctx) {
  // Permission check
  if (!checkPermission(ctx, auth::Permission::AdminConfig)) {
    return ctx.sendError(403, "Permission denied: admin:config required");
  }

  try {
    // Extract key from path parameters
    KJ_IF_SOME(key, ctx.path_params.find("key"_kj)) {
      auto state = state_.lockExclusive();

      // Check if key is read-only
      for (const auto& readonly_key : state->readonly_keys) {
        if (readonly_key == key) {
          auto builder = core::JsonBuilder::object();
          builder.put("status", "error");
          builder.put("error", "Cannot delete read-only configuration key");
          kj::String json_body = builder.build();
          return ctx.sendJson(403, json_body);
        }
      }

      // Delete configuration key
      if (state->config_values.erase(key)) {
        // Log audit event (fire-and-forget)
        kj::String userId = kj::str("unknown");
        KJ_IF_SOME(auth, ctx.authInfo) {
          userId = kj::heapString(auth.user_id);
        }
        (void)audit_.log(audit::AuditLogType::Access, kj::str("CONFIG_KEY_DELETE"), kj::mv(userId),
                         kj::heapString(ctx.clientIP), kj::none);

        auto builder = core::JsonBuilder::object();
        builder.put("status", "success");
        builder.put("key", key.cStr());
        builder.put("deleted_at", getCurrentTimestamp().cStr());

        kj::String json_body = builder.build();
        return ctx.sendJson(200, json_body);
      } else {
        return ctx.sendError(404, "Configuration key not found");
      }
    }
    else {
      return ctx.sendError(400, "Missing key parameter");
    }
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Error in config handler", e);
    return ctx.sendError(500, "Internal Server Error"_kj);
  } catch (...) {
    KJ_LOG(ERROR, "Unknown error in config handler");
    return ctx.sendError(500, "Internal Server Error"_kj);
  }
}

void ConfigHandler::initialize_defaults() {
  auto state = state_.lockExclusive();

  // Add default configuration values
  state->config_values.insert(kj::heapString("gateway.version"),
                              ConfigValue{kj::heapString("1.0.0")});
  state->config_values.insert(kj::heapString("gateway.name"),
                              ConfigValue{kj::heapString("VeloZ Gateway")});
  state->config_values.insert(kj::heapString("gateway.max_connections"), ConfigValue{1000.0});
  state->config_values.insert(kj::heapString("gateway.request_timeout_ms"), ConfigValue{30000.0});
  state->config_values.insert(kj::heapString("trading.max_order_size"), ConfigValue{100.0});
  state->config_values.insert(kj::heapString("trading.min_order_size"), ConfigValue{0.001});
  state->config_values.insert(kj::heapString("trading.default_order_type"),
                              ConfigValue{kj::heapString("limit")});
  state->config_values.insert(kj::heapString("risk.enable_position_limits"), ConfigValue{true});
  state->config_values.insert(kj::heapString("risk.max_position_size"), ConfigValue{1000.0});
  state->config_values.insert(kj::heapString("audit.enabled"), ConfigValue{true});
  state->config_values.insert(kj::heapString("audit.log_level"),
                              ConfigValue{kj::heapString("info")});

  // Add read-only keys
  state->readonly_keys.add(kj::heapString("gateway.version"));
  state->readonly_keys.add(kj::heapString("gateway.name"));
}

void ConfigHandler::set_config(kj::StringPtr key, ConfigValue value) {
  auto state = state_.lockExclusive();
  state->config_values.upsert(kj::str(key), kj::mv(value));
}

kj::Maybe<ConfigValue> ConfigHandler::get_config(kj::StringPtr key) const {
  auto state = state_.lockShared();
  KJ_IF_SOME(valuePtr, state->config_values.find(key)) {
    // Return a copy of the value
    ConfigValue copy;
    copy.type = valuePtr.type;
    switch (valuePtr.type) {
    case ConfigValue::Type::String:
      KJ_IF_SOME(str, valuePtr.data.tryGet<ConfigValue::StringData>()) {
        copy = ConfigValue{kj::heapString(str.value)};
      }
      break;
    case ConfigValue::Type::Number:
      KJ_IF_SOME(num, valuePtr.data.tryGet<ConfigValue::NumberData>()) {
        copy = ConfigValue{num.value};
      }
      break;
    case ConfigValue::Type::Boolean:
      KJ_IF_SOME(flag, valuePtr.data.tryGet<ConfigValue::BooleanData>()) {
        copy = ConfigValue{flag.value};
      }
      break;
    default:
      break;
    }
    return kj::mv(copy);
  }
  return kj::none;
}

bool ConfigHandler::is_readonly(kj::StringPtr key) const {
  auto state = state_.lockShared();
  for (const auto& readonly_key : state->readonly_keys) {
    if (readonly_key == key) {
      return true;
    }
  }
  return false;
}

kj::String ConfigHandler::formatConfigJson(const ConfigValue& value) {
  switch (value.type) {
  case ConfigValue::Type::String:
    KJ_IF_SOME(str, value.data.tryGet<ConfigValue::StringData>()) {
      return kj::str("\"", escapeJson(str.value), "\"");
    }
    return kj::str("null");

  case ConfigValue::Type::Number:
    KJ_IF_SOME(num, value.data.tryGet<ConfigValue::NumberData>()) {
      return kj::str(num.value);
    }
    return kj::str("null");

  case ConfigValue::Type::Boolean:
    KJ_IF_SOME(flag, value.data.tryGet<ConfigValue::BooleanData>()) {
      return kj::str(flag.value ? "true" : "false");
    }
    return kj::str("null");

  default:
    return kj::str("null");
  }
}

kj::String ConfigHandler::getCurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time));
  return kj::heapString(buffer);
}

bool ConfigHandler::checkPermission(RequestContext& ctx, kj::StringPtr permission) {
  KJ_IF_SOME(auth, ctx.authInfo) {
    for (const auto& perm : auth.permissions) {
      if (perm == permission) {
        return true;
      }
    }
  }
  return false;
}

bool ConfigHandler::checkPermission(RequestContext& ctx, auth::Permission permission) {
  // Convert Permission enum to string for comparison
  kj::String perm_str;
  switch (permission) {
  case auth::Permission::ReadMarket:
    perm_str = kj::str("read:market");
    break;
  case auth::Permission::ReadOrders:
    perm_str = kj::str("read:orders");
    break;
  case auth::Permission::ReadAccount:
    perm_str = kj::str("read:account");
    break;
  case auth::Permission::ReadConfig:
    perm_str = kj::str("read:config");
    break;
  case auth::Permission::WriteOrders:
    perm_str = kj::str("write:orders");
    break;
  case auth::Permission::WriteCancel:
    perm_str = kj::str("write:cancel");
    break;
  case auth::Permission::AdminKeys:
    perm_str = kj::str("admin:keys");
    break;
  case auth::Permission::AdminUsers:
    perm_str = kj::str("admin:users");
    break;
  case auth::Permission::AdminConfig:
    perm_str = kj::str("admin:config");
    break;
  default:
    return false;
  }
  return checkPermission(ctx, perm_str);
}

kj::Maybe<ConfigValue> ConfigHandler::parseConfigValue(kj::StringPtr json) {
  try {
    auto doc = core::JsonDocument::parse(json);
    auto root = doc.root();

    if (root.is_string()) {
      return ConfigValue{root.get_string()};
    } else if (root.is_real() || root.is_int() || root.is_uint()) {
      return ConfigValue{root.get_double()};
    } else if (root.is_bool()) {
      return ConfigValue{root.get_bool()};
    }

    return kj::none;
  } catch (...) {
    return kj::none;
  }
}

bool ConfigHandler::validateConfigKey(kj::StringPtr key, kj::String& error) {
  // Key must not be empty
  if (key.size() == 0) {
    error = kj::str("Configuration key cannot be empty");
    return false;
  }

  // Key must use dot notation for hierarchy
  // Valid format: section.name (e.g., "gateway.max_connections")
  if (key.findFirst('.') == kj::none) {
    error = kj::str("Configuration key must use dot notation (e.g., 'section.name')");
    return false;
  }

  // Key must start with a letter
  char first = key[0];
  if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z'))) {
    error = kj::str("Configuration key must start with a letter");
    return false;
  }

  // Key must only contain alphanumeric, dots, and underscores
  for (char c : key) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
          c == '_' || c == '-')) {
      error = kj::str("Configuration key contains invalid characters");
      return false;
    }
  }

  return true;
}

} // namespace veloz::gateway
