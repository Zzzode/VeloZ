#pragma once

#include "auth/rbac.h"
#include "request_context.h"

#include <kj/async-io.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

// Forward declarations
namespace audit {
class AuditLogger;
}

/**
 * Configuration value with type information.
 */
struct ConfigValue {
  enum class Type { String, Number, Boolean, Array, Object };
  Type type;

  // Value storage (using kj::OneOf for type-safe union)
  struct StringData {
    kj::String value;
    bool operator==(const StringData& other) const {
      return value == other.value;
    }
  };
  struct NumberData {
    double value;
    bool operator==(const NumberData& other) const {
      return value == other.value;
    }
  };
  struct BooleanData {
    bool value;
    bool operator==(const BooleanData& other) const {
      return value == other.value;
    }
  };

  struct EmptyData {
    bool operator==(const EmptyData&) const {
      return true;
    }
  };

  kj::OneOf<EmptyData, StringData, NumberData, BooleanData> data;

  // Constructors
  ConfigValue() : type(Type::String), data(StringData{kj::heapString("")}) {}
  explicit ConfigValue(kj::String str) : type(Type::String), data(StringData{kj::mv(str)}) {}
  explicit ConfigValue(double num) : type(Type::Number), data(NumberData{num}) {}
  explicit ConfigValue(bool flag) : type(Type::Boolean), data(BooleanData{flag}) {}

  // Move-only due to kj::String
  ConfigValue(ConfigValue&&) noexcept = default;
  ConfigValue& operator=(ConfigValue&&) noexcept = default;
  ConfigValue(const ConfigValue&) = delete;
  ConfigValue& operator=(const ConfigValue&) = delete;
};

/**
 * Configuration handler.
 *
 * Handles gateway configuration queries and updates (/api/config).
 * Endpoints:
 * - GET /api/config - Get configuration
 * - GET /api/config/{key} - Get specific configuration value
 * - POST /api/config - Update configuration
 * - POST /api/config/{key} - Update specific configuration value
 * - DELETE /api/config/{key} - Delete configuration value
 */
class ConfigHandler {
public:
  /**
   * @brief Construct ConfigHandler with dependencies
   *
   * @param audit Audit logger for access logging
   */
  explicit ConfigHandler(audit::AuditLogger& audit);

  /**
   * @brief Handle GET /api/config
   *
   * Returns all configuration values.
   * Requires ReadConfig permission.
   *
   * Response format:
   * {
   *   "status": "success",
   *   "data": {
   *     "gateway.version": "1.0.0",
   *     "gateway.max_connections": 1000,
   *     ...
   *   }
   * }
   *
   * @param ctx Request context with auth info
   * @return Promise that completes when response is sent
   */
  kj::Promise<void> handleGetConfig(RequestContext& ctx);

  // GET /api/config/{key} - Get specific configuration value
  kj::Promise<void> handleGetConfigKey(RequestContext& ctx);

  // POST /api/config - Update configuration (batch)
  kj::Promise<void> handleUpdateConfig(RequestContext& ctx);

  // POST /api/config/{key} - Update specific configuration value
  kj::Promise<void> handleUpdateConfigKey(RequestContext& ctx);

  // DELETE /api/config/{key} - Delete configuration value
  kj::Promise<void> handleDeleteConfigKey(RequestContext& ctx);

  /**
   * @brief Initialize default configuration values
   */
  void initialize_defaults();

  /**
   * @brief Set a configuration value
   */
  void set_config(kj::StringPtr key, ConfigValue value);

  /**
   * @brief Get a configuration value
   */
  [[nodiscard]] kj::Maybe<ConfigValue> get_config(kj::StringPtr key) const;

  /**
   * @brief Check if configuration key is read-only
   */
  [[nodiscard]] bool is_readonly(kj::StringPtr key) const;

  /**
   * @brief Validate configuration key format
   */
  bool validateConfigKey(kj::StringPtr key, kj::String& error);

  // Public methods for testing
  kj::String formatConfigJson(const ConfigValue& value);
  kj::String getCurrentTimestamp();

private:
  audit::AuditLogger& audit_;

  // Configuration storage (thread-safe)
  struct ConfigState {
    kj::HashMap<kj::String, ConfigValue> config_values;
    kj::Vector<kj::String> readonly_keys;
  };
  kj::MutexGuarded<ConfigState> state_;

  /**
   * @brief Check if user has required permission (string)
   */
  bool checkPermission(RequestContext& ctx, kj::StringPtr permission);

  /**
   * @brief Check if user has required permission (enum)
   */
  bool checkPermission(RequestContext& ctx, auth::Permission permission);

  /**
   * @brief Parse configuration value from JSON string
   */
  kj::Maybe<ConfigValue> parseConfigValue(kj::StringPtr json);
};

} // namespace veloz::gateway
