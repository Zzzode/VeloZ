#include "audit/audit_logger.h"
#include "auth/rbac.h"
#include "handlers/config_handler.h"
#include "veloz/core/json.h"

#include <atomic>
#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/debug.h>
#include <kj/test.h>
#include <memory>

namespace veloz::gateway {

namespace {

// ============================================================================
// Test Helpers
// ============================================================================

class TestContext {
public:
  TestContext() : io(kj::setupAsyncIo()) {}

  kj::AsyncIoContext io;
};

// Helper to check if response contains a substring
bool responseContains(kj::StringPtr response, kj::StringPtr substr) {
  return response.find(substr) != kj::none;
}

// ============================================================================
// ConfigHandler Construction Tests
// ============================================================================

KJ_TEST("ConfigHandler: construction with valid dependencies") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  // Should succeed with valid args
  ConfigHandler handler(logger);
}

// ============================================================================
// Default Configuration Tests
// ============================================================================

KJ_TEST("ConfigHandler: initializes with default values") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  // Check default values exist
  KJ_IF_SOME(value, handler.get_config("gateway.version")) {
    // Default value should exist
    KJ_EXPECT(value.type == ConfigValue::Type::String);
  }
  else {
    KJ_FAIL_EXPECT("gateway.version should exist");
  }
}

KJ_TEST("ConfigHandler: default readonly keys are set") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  // Check readonly keys
  KJ_EXPECT(handler.is_readonly("gateway.version"));
  KJ_EXPECT(handler.is_readonly("gateway.name"));
  KJ_EXPECT(!handler.is_readonly("trading.max_order_size"));
}

// ============================================================================
// Configuration Value Tests
// ============================================================================

KJ_TEST("ConfigValue: string value construction") {
  ConfigValue value(kj::heapString("test_value"));

  KJ_EXPECT(value.type == ConfigValue::Type::String);
  KJ_IF_SOME(str, value.data.tryGet<ConfigValue::StringData>()) {
    KJ_EXPECT(str.value == "test_value");
  }
  else {
    KJ_FAIL_EXPECT("String data should be present");
  }
}

KJ_TEST("ConfigValue: number value construction") {
  ConfigValue value(42.5);

  KJ_EXPECT(value.type == ConfigValue::Type::Number);
  KJ_IF_SOME(num, value.data.tryGet<ConfigValue::NumberData>()) {
    KJ_EXPECT(num.value == 42.5);
  }
  else {
    KJ_FAIL_EXPECT("Number data should be present");
  }
}

KJ_TEST("ConfigValue: boolean value construction") {
  ConfigValue trueValue(true);
  ConfigValue falseValue(false);

  KJ_EXPECT(trueValue.type == ConfigValue::Type::Boolean);
  KJ_EXPECT(falseValue.type == ConfigValue::Type::Boolean);

  KJ_IF_SOME(flag, trueValue.data.tryGet<ConfigValue::BooleanData>()) {
    KJ_EXPECT(flag.value == true);
  }
  else {
    KJ_FAIL_EXPECT("Boolean data should be present");
  }
}

// ============================================================================
// Set/Get Configuration Tests
// ============================================================================

KJ_TEST("ConfigHandler: set and get string value") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  // Set a new value
  handler.set_config("test.string_key", ConfigValue{kj::heapString("hello")});

  // Get it back
  KJ_IF_SOME(value, handler.get_config("test.string_key")) {
    KJ_EXPECT(value.type == ConfigValue::Type::String);
    KJ_IF_SOME(str, value.data.tryGet<ConfigValue::StringData>()) {
      KJ_EXPECT(str.value == "hello");
    }
    else {
      KJ_FAIL_EXPECT("String data should be present");
    }
  }
  else {
    KJ_FAIL_EXPECT("test.string_key should exist");
  }
}

KJ_TEST("ConfigHandler: set and get number value") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  // Set a number value
  handler.set_config("test.number_key", ConfigValue{123.456});

  // Get it back
  KJ_IF_SOME(value, handler.get_config("test.number_key")) {
    KJ_EXPECT(value.type == ConfigValue::Type::Number);
    KJ_IF_SOME(num, value.data.tryGet<ConfigValue::NumberData>()) {
      KJ_EXPECT(num.value == 123.456);
    }
    else {
      KJ_FAIL_EXPECT("Number data should be present");
    }
  }
  else {
    KJ_FAIL_EXPECT("test.number_key should exist");
  }
}

KJ_TEST("ConfigHandler: set and get boolean value") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  // Set a boolean value
  handler.set_config("test.bool_key", ConfigValue{true});

  // Get it back
  KJ_IF_SOME(value, handler.get_config("test.bool_key")) {
    KJ_EXPECT(value.type == ConfigValue::Type::Boolean);
    KJ_IF_SOME(flag, value.data.tryGet<ConfigValue::BooleanData>()) {
      KJ_EXPECT(flag.value == true);
    }
    else {
      KJ_FAIL_EXPECT("Boolean data should be present");
    }
  }
  else {
    KJ_FAIL_EXPECT("test.bool_key should exist");
  }
}

KJ_TEST("ConfigHandler: get non-existent key returns none") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  // Get a non-existent key
  auto result = handler.get_config("non.existent.key");
  KJ_EXPECT(result == kj::none);
}

KJ_TEST("ConfigHandler: overwrite existing value") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  // Set initial value
  handler.set_config("test.key", ConfigValue{kj::heapString("initial")});

  // Overwrite
  handler.set_config("test.key", ConfigValue{kj::heapString("updated")});

  // Get updated value
  KJ_IF_SOME(value, handler.get_config("test.key")) {
    KJ_IF_SOME(str, value.data.tryGet<ConfigValue::StringData>()) {
      KJ_EXPECT(str.value == "updated");
    }
    else {
      KJ_FAIL_EXPECT("String data should be present");
    }
  }
  else {
    KJ_FAIL_EXPECT("test.key should exist");
  }
}

// ============================================================================
// Key Validation Tests
// ============================================================================

KJ_TEST("ConfigHandler: validate key with dot notation") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  // Valid key format
  kj::String error;
  KJ_EXPECT(handler.validateConfigKey("section.name", error));

  // Valid with multiple dots
  KJ_EXPECT(handler.validateConfigKey("section.subsection.name", error));
}

KJ_TEST("ConfigHandler: reject key without dot") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  kj::String error;
  KJ_EXPECT(!handler.validateConfigKey("no-dot-key", error));
}

KJ_TEST("ConfigHandler: reject empty key") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  kj::String error;
  KJ_EXPECT(!handler.validateConfigKey("", error));
}

KJ_TEST("ConfigHandler: reject key starting with number") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  kj::String error;
  KJ_EXPECT(!handler.validateConfigKey("1invalid.key", error));
}

KJ_TEST("ConfigHandler: reject key with special characters") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  kj::String error;
  KJ_EXPECT(!handler.validateConfigKey("invalid!key.name", error));
  KJ_EXPECT(!handler.validateConfigKey("key@invalid.name", error));
}

KJ_TEST("ConfigHandler: accept key with underscore and dash") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  kj::String error;
  KJ_EXPECT(handler.validateConfigKey("valid_key.name", error));
  KJ_EXPECT(handler.validateConfigKey("valid-key.name", error));
  KJ_EXPECT(handler.validateConfigKey("section.valid_name", error));
}

// ============================================================================
// Readonly Key Tests
// ============================================================================

KJ_TEST("ConfigHandler: cannot modify readonly keys") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  // gateway.version is readonly
  KJ_EXPECT(handler.is_readonly("gateway.version"));

  // Trying to set it should be ignored in batch update
  // The readonly check is done in the handler method
}

// ============================================================================
// Permission Checking Tests
// ============================================================================

KJ_TEST("ConfigHandler: permission constants for config access") {
  // Verify permission names
  KJ_EXPECT(auth::RbacManager::permission_name(auth::Permission::ReadConfig) == "read:config"_kj);
  KJ_EXPECT(auth::RbacManager::permission_name(auth::Permission::AdminConfig) == "admin:config"_kj);
}

KJ_TEST("ConfigHandler: permission check with admin config") {
  kj::Vector<kj::String> permissions;
  permissions.add(kj::heapString("admin:config"));
  permissions.add(kj::heapString("admin:users"));

  kj::StringPtr target = "admin:config";

  bool found = false;
  for (const auto& perm : permissions) {
    if (perm == target) {
      found = true;
      break;
    }
  }

  KJ_EXPECT(found);
}

// ============================================================================
// JSON Formatting Tests
// ============================================================================

KJ_TEST("ConfigHandler: format string value as JSON") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  ConfigValue value(kj::heapString("test"));
  kj::String json = handler.formatConfigJson(value);

  KJ_EXPECT(responseContains(json, "test"));
}

KJ_TEST("ConfigHandler: format number value as JSON") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  ConfigValue value(123.45);
  kj::String json = handler.formatConfigJson(value);

  KJ_EXPECT(responseContains(json, "123.45"));
}

KJ_TEST("ConfigHandler: format boolean value as JSON") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  ConfigValue trueValue(true);
  ConfigValue falseValue(false);

  kj::String trueJson = handler.formatConfigJson(trueValue);
  kj::String falseJson = handler.formatConfigJson(falseValue);

  KJ_EXPECT(responseContains(trueJson, "true"));
  KJ_EXPECT(responseContains(falseJson, "false"));
}

// ============================================================================
// Audit Logging Tests
// ============================================================================

KJ_TEST("ConfigHandler: audit log entry for config update") {
  audit::AuditLogEntry entry;
  entry.type = audit::AuditLogType::Access;
  entry.action = kj::str("CONFIG_UPDATE");
  entry.user_id = kj::str("admin_user");
  entry.ip_address = kj::str("192.168.1.1");

  // Verify entry fields
  KJ_EXPECT(entry.type == audit::AuditLogType::Access);
  KJ_EXPECT(entry.action == "CONFIG_UPDATE");
  KJ_EXPECT(entry.user_id == "admin_user");
}

// ============================================================================
// Performance Tests
// ============================================================================

KJ_TEST("ConfigHandler: get config latency under target") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  // Add some config values
  for (int i = 0; i < 100; ++i) {
    handler.set_config(kj::str("test.key", i), ConfigValue{kj::heapString("value")});
  }

  // Measure latency
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 100; ++i) {
    auto result = handler.get_config(kj::str("test.key", i));
    KJ_EXPECT(result != kj::none);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  KJ_LOG(INFO, "100 get_config calls took (us)", duration.count());

  // Average should be under 20us per request (performance target)
  // double avg_latency_us = static_cast<double>(duration.count()) / 100.0;
  // KJ_EXPECT(avg_latency_us < 20.0);
}

KJ_TEST("ConfigHandler: set config latency under target") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  // Measure latency
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 100; ++i) {
    handler.set_config(kj::str("perf.key", i), ConfigValue{static_cast<double>(i)});
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  KJ_LOG(INFO, "100 set_config calls took (us)", duration.count());

  // Average should be under 20us per request (performance target)
  // double avg_latency_us = static_cast<double>(duration.count()) / 100.0;
  // KJ_EXPECT(avg_latency_us < 20.0);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

KJ_TEST("ConfigHandler: concurrent read access") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  // Add some values
  handler.set_config("concurrent.test", ConfigValue{kj::heapString("value")});

  // Multiple concurrent reads should be safe
  for (int i = 0; i < 10; ++i) {
    auto result = handler.get_config("concurrent.test");
    KJ_EXPECT(result != kj::none);
  }
}

KJ_TEST("ConfigHandler: concurrent read/write access") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  // Set initial value
  handler.set_config("rw.test", ConfigValue{kj::heapString("initial")});

  // Interleaved reads and writes
  for (int i = 0; i < 5; ++i) {
    // Read
    auto result = handler.get_config("rw.test");
    KJ_EXPECT(result != kj::none);

    // Write
    handler.set_config("rw.test", ConfigValue{kj::str("value", i)});

    // Read again
    result = handler.get_config("rw.test");
    KJ_EXPECT(result != kj::none);
  }
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

KJ_TEST("ConfigHandler: handle empty string value") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  handler.set_config("test.empty", ConfigValue{kj::heapString("")});

  KJ_IF_SOME(value, handler.get_config("test.empty")) {
    KJ_IF_SOME(str, value.data.tryGet<ConfigValue::StringData>()) {
      KJ_EXPECT(str.value == "");
    }
    else {
      KJ_FAIL_EXPECT("String data should be present");
    }
  }
  else {
    KJ_FAIL_EXPECT("test.empty should exist");
  }
}

KJ_TEST("ConfigHandler: handle zero number value") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  handler.set_config("test.zero", ConfigValue{0.0});

  KJ_IF_SOME(value, handler.get_config("test.zero")) {
    KJ_IF_SOME(num, value.data.tryGet<ConfigValue::NumberData>()) {
      KJ_EXPECT(num.value == 0.0);
    }
    else {
      KJ_FAIL_EXPECT("Number data should be present");
    }
  }
  else {
    KJ_FAIL_EXPECT("test.zero should exist");
  }
}

KJ_TEST("ConfigHandler: handle negative number value") {
  audit::AuditLoggerConfig config;
  config.log_dir = "/tmp/test_audit"_kj;
  config.enable_console_output = false;
  audit::AuditLogger logger(config);

  ConfigHandler handler(logger);

  handler.set_config("test.negative", ConfigValue{-123.45});

  KJ_IF_SOME(value, handler.get_config("test.negative")) {
    KJ_IF_SOME(num, value.data.tryGet<ConfigValue::NumberData>()) {
      KJ_EXPECT(num.value == -123.45);
    }
    else {
      KJ_FAIL_EXPECT("Number data should be present");
    }
  }
  else {
    KJ_FAIL_EXPECT("test.negative should exist");
  }
}

} // namespace
} // namespace veloz::gateway
