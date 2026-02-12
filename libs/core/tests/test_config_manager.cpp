#include "veloz/core/config_manager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>

using namespace veloz::core;

class ConfigManagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Clean up test config files
    try {
      std::filesystem::remove_all("test_configs");
    } catch (...) {
    }

    std::filesystem::create_directories("test_configs");
  }

  void TearDown() override {
    // Clean up test config files
    try {
      std::filesystem::remove_all("test_configs");
    } catch (...) {
    }
  }
};

// ============================================================================
// ConfigItem Tests
// ============================================================================

TEST_F(ConfigManagerTest, ConfigItemBuilder) {
  auto item =
      ConfigItem<int>::Builder("test_item", "Test item").default_value(42).required(false).build();

  EXPECT_EQ(std::string(item->key()), "test_item");
  EXPECT_EQ(std::string(item->description()), "Test item");
  EXPECT_FALSE(item->is_required());
  EXPECT_TRUE(item->has_default());
  EXPECT_TRUE(item->is_set());
}

TEST_F(ConfigManagerTest, ConfigItemGetValue) {
  auto item = ConfigItem<int>::Builder("test", "Test").default_value(100).build();

  auto value = item->get();
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value.value(), 100);

  EXPECT_EQ(item->value(), 100);
}

TEST_F(ConfigManagerTest, ConfigItemSetValue) {
  auto item = ConfigItem<int>::Builder("test", "Test").default_value(100).build();

  EXPECT_TRUE(item->set(200));
  EXPECT_EQ(item->value(), 200);
}

TEST_F(ConfigManagerTest, ConfigItemValidator) {
  auto item = ConfigItem<int>::Builder("test", "Test")
                  .validator([](const int& v) { return v >= 0 && v <= 100; })
                  .build();

  EXPECT_TRUE(item->set(50));
  EXPECT_EQ(item->value(), 50);

  EXPECT_FALSE(item->set(150)); // Should fail validation
  EXPECT_EQ(item->value(), 50); // Value unchanged
}

TEST_F(ConfigManagerTest, ConfigItemCallback) {
  int callback_count = 0;
  int old_value = 0;
  int new_value = 0;

  auto item = ConfigItem<int>::Builder("test", "Test")
                  .default_value(10)
                  .on_change([&](const int& old_v, const int& new_v) {
                    ++callback_count;
                    old_value = old_v;
                    new_value = new_v;
                  })
                  .build();

  EXPECT_EQ(callback_count, 0);

  item->set(20);
  EXPECT_EQ(callback_count, 1);
  EXPECT_EQ(old_value, 10);
  EXPECT_EQ(new_value, 20);
}

TEST_F(ConfigManagerTest, ConfigItemReset) {
  auto item = ConfigItem<int>::Builder("test", "Test").default_value(100).build();

  item->set(200);
  EXPECT_EQ(item->value(), 200);

  item->reset();
  EXPECT_EQ(item->value(), 100);
}

TEST_F(ConfigManagerTest, ConfigItemToString) {
  auto int_item = ConfigItem<int>::Builder("int", "Int").default_value(42).build();
  EXPECT_EQ(int_item->to_string(), "42");

  auto string_item = ConfigItem<std::string>::Builder("str", "Str").default_value("hello").build();
  EXPECT_EQ(string_item->to_string(), "\"hello\"");

  auto bool_item = ConfigItem<bool>::Builder("bool", "Bool").default_value(true).build();
  EXPECT_EQ(bool_item->to_string(), "true");
}

TEST_F(ConfigManagerTest, ConfigItemFromString) {
  auto int_item = ConfigItem<int>::Builder("int", "Int").build();
  EXPECT_TRUE(int_item->from_string("42"));
  EXPECT_EQ(int_item->value(), 42);

  auto bool_item = ConfigItem<bool>::Builder("bool", "Bool").build();
  EXPECT_TRUE(bool_item->from_string("true"));
  EXPECT_TRUE(bool_item->value());
  EXPECT_TRUE(bool_item->from_string("false"));
  EXPECT_FALSE(bool_item->value());

  auto string_item = ConfigItem<std::string>::Builder("str", "Str").build();
  EXPECT_TRUE(string_item->from_string("hello"));
  EXPECT_EQ(string_item->value(), "hello");
}

TEST_F(ConfigManagerTest, ConfigItemArray) {
  auto array_item = ConfigItem<std::vector<int>>::Builder("array", "Array")
                        .default_value(std::vector<int>{1, 2, 3})
                        .build();

  auto value = array_item->get();
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value->size(), 3);
  EXPECT_EQ((*value)[0], 1);
  EXPECT_EQ((*value)[1], 2);
  EXPECT_EQ((*value)[2], 3);

  std::string array_str = array_item->to_string();
  EXPECT_TRUE(array_str.find("[") != std::string::npos);
}

// ============================================================================
// ConfigGroup Tests
// ============================================================================

TEST_F(ConfigManagerTest, ConfigGroupAddItem) {
  ConfigGroup group("test_group", "Test group");

  auto item1 = ConfigItem<int>::Builder("item1", "Item 1").default_value(1).build();
  auto item2 = ConfigItem<std::string>::Builder("item2", "Item 2").default_value("test").build();

  group.add_item(std::move(item1));
  group.add_item(std::move(item2));

  auto* found_item1 = group.get_item<int>("item1");
  ASSERT_NE(found_item1, nullptr);
  EXPECT_EQ(found_item1->value(), 1);

  auto* found_item2 = group.get_item<std::string>("item2");
  ASSERT_NE(found_item2, nullptr);
  EXPECT_EQ(found_item2->value(), "test");
}

TEST_F(ConfigManagerTest, ConfigGroupSubgroups) {
  ConfigGroup root("root", "Root group");
  auto sub1 = std::make_unique<ConfigGroup>("sub1", "Subgroup 1");
  auto sub2 = std::make_unique<ConfigGroup>("sub2", "Subgroup 2");

  root.add_group(std::move(sub1));
  root.add_group(std::move(sub2));

  auto* found_sub1 = root.get_group("sub1");
  ASSERT_NE(found_sub1, nullptr);
  EXPECT_EQ(std::string(found_sub1->name()), "sub1");

  auto* found_sub2 = root.get_group("sub2");
  ASSERT_NE(found_sub2, nullptr);
  EXPECT_EQ(std::string(found_sub2->name()), "sub2");
}

TEST_F(ConfigManagerTest, ConfigGroupValidate) {
  ConfigGroup group("test", "Test");

  auto optional_item = ConfigItem<int>::Builder("opt", "Optional").default_value(1).build();
  auto required_item = ConfigItem<int>::Builder("req", "Required").required(true).build();

  ConfigItem<int>* required_item_ptr = required_item.get(); // 保存原始指针

  group.add_item(std::move(optional_item));
  group.add_item(std::move(required_item));

  EXPECT_FALSE(group.validate()); // Required item not set
  EXPECT_TRUE(group.validation_errors().size() > 0);

  required_item_ptr->set(10); // 使用保存的指针
  EXPECT_TRUE(group.validate());
  EXPECT_TRUE(group.validation_errors().empty());
}

TEST_F(ConfigManagerTest, ConfigGroupGetItems) {
  ConfigGroup group("test", "Test");

  group.add_item(ConfigItem<int>::Builder("item1", "1").default_value(1).build());
  group.add_item(ConfigItem<std::string>::Builder("item2", "2").default_value("test").build());

  auto items = group.get_items();
  EXPECT_EQ(items.size(), 2);
}

// ============================================================================
// ConfigManager Tests
// ============================================================================

TEST_F(ConfigManagerTest, ConfigManagerBasic) {
  ConfigManager manager("test");

  auto* root = manager.root_group();
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(std::string(root->name()), "root");
}

TEST_F(ConfigManagerTest, ConfigManagerLoadFromJson) {
  std::ofstream ofs("test_configs/config.json");
  ofs << R"({
    "timeout": 30,
    "max_connections": 100,
    "enabled": true,
    "server_name": "test-server"
  })";
  ofs.close();

  ConfigManager manager("test");
  EXPECT_TRUE(manager.load_from_json("test_configs/config.json"));
}

TEST_F(ConfigManagerTest, ConfigManagerLoadFromJsonString) {
  std::string json = R"({
    "value1": 42,
    "value2": "hello",
    "value3": false
  })";

  ConfigManager manager("test");
  EXPECT_TRUE(manager.load_from_json_string(json));
}

TEST_F(ConfigManagerTest, ConfigManagerFindItem) {
  ConfigManager manager("test");
  auto* root = manager.root_group();

  root->add_item(ConfigItem<int>::Builder("test_item", "Test").default_value(100).build());

  auto* item = manager.find_item("test_item");
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(std::string(item->key()), "test_item");

  auto* typed_item = manager.find_item<int>("test_item");
  ASSERT_NE(typed_item, nullptr);
  EXPECT_EQ(typed_item->value(), 100);
}

TEST_F(ConfigManagerTest, ConfigManagerSaveToJson) {
  ConfigManager manager("test");
  auto* root = manager.root_group();

  root->add_item(ConfigItem<int>::Builder("int_val", "Int").default_value(42).build());
  root->add_item(ConfigItem<std::string>::Builder("str_val", "Str").default_value("test").build());
  root->add_item(ConfigItem<bool>::Builder("bool_val", "Bool").default_value(true).build());

  EXPECT_TRUE(manager.save_to_json("test_configs/saved.json"));

  EXPECT_TRUE(std::filesystem::exists("test_configs/saved.json"));

  // Verify content
  std::ifstream ifs("test_configs/saved.json");
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  EXPECT_TRUE(content.find("\"int_val\"") != std::string::npos);
  EXPECT_TRUE(content.find("\"str_val\"") != std::string::npos);
  EXPECT_TRUE(content.find("\"bool_val\"") != std::string::npos);
}

TEST_F(ConfigManagerTest, ConfigManagerToJsonString) {
  ConfigManager manager("test");
  auto* root = manager.root_group();

  root->add_item(ConfigItem<int>::Builder("test", "Test").default_value(42).build());

  std::string json = manager.to_json();

  EXPECT_FALSE(json.empty());
  EXPECT_TRUE(json.find("test") != std::string::npos);
}

TEST_F(ConfigManagerTest, ConfigManagerNestedGroups) {
  ConfigManager manager("test");
  auto* root = manager.root_group();

  auto db_group = std::make_unique<ConfigGroup>("database", "Database config");
  db_group->add_item(
      ConfigItem<std::string>::Builder("host", "Host").default_value("localhost").build());
  db_group->add_item(ConfigItem<int>::Builder("port", "Port").default_value(5432).build());

  root->add_group(std::move(db_group));

  // Find item with path notation
  auto* host_item = manager.find_item<std::string>("database.host");
  ASSERT_NE(host_item, nullptr);
  EXPECT_EQ(host_item->value(), "localhost");

  auto* port_item = manager.find_item<int>("database.port");
  ASSERT_NE(port_item, nullptr);
  EXPECT_EQ(port_item->value(), 5432);
}

TEST_F(ConfigManagerTest, ConfigManagerHotReload) {
  ConfigManager manager("test");
  auto* root = manager.root_group();

  root->add_item(ConfigItem<int>::Builder("reloaded", "Reload test").default_value(1).build());

  int callback_count = 0;
  manager.add_hot_reload_callback([&] { ++callback_count; });

  manager.set_hot_reload_enabled(true);
  EXPECT_TRUE(manager.hot_reload_enabled());

  // Trigger reload manually
  manager.trigger_hot_reload();
  EXPECT_EQ(callback_count, 1);
}

TEST_F(ConfigManagerTest, ConfigManagerLoadAndReload) {
  ConfigManager manager("test");
  auto* root = manager.root_group();

  root->add_item(ConfigItem<int>::Builder("timeout", "Timeout").default_value(10).build());

  // Load initial config
  std::ofstream ofs("test_configs/reload.json");
  ofs << R"({ "timeout": 30 })";
  ofs.close();

  EXPECT_TRUE(manager.load_from_json("test_configs/reload.json"));

  // Verify value was updated
  auto* timeout_item = manager.find_item<int>("timeout");
  ASSERT_NE(timeout_item, nullptr);
  EXPECT_EQ(timeout_item->value(), 30);

  // Reload with different value
  ofs.open("test_configs/reload.json");
  ofs << R"({ "timeout": 60 })";
  ofs.close();

  EXPECT_TRUE(manager.load_from_json("test_configs/reload.json", true));
  EXPECT_EQ(timeout_item->value(), 60);
}

// ============================================================================
// ConfigItemType Tests
// ============================================================================

TEST_F(ConfigManagerTest, ConfigItemTypeTraits) {
  EXPECT_EQ(ConfigTypeTraits<bool>::type, ConfigItemType::Bool);
  EXPECT_EQ(ConfigTypeTraits<int>::type, ConfigItemType::Int);
  EXPECT_EQ(ConfigTypeTraits<int64_t>::type, ConfigItemType::Int64);
  EXPECT_EQ(ConfigTypeTraits<double>::type, ConfigItemType::Double);
  EXPECT_EQ(ConfigTypeTraits<std::string>::type, ConfigItemType::String);
  EXPECT_EQ(ConfigTypeTraits<std::vector<bool>>::type, ConfigItemType::BoolArray);
  EXPECT_EQ(ConfigTypeTraits<std::vector<int>>::type, ConfigItemType::IntArray);
  EXPECT_EQ(ConfigTypeTraits<std::vector<int64_t>>::type, ConfigItemType::Int64Array);
  EXPECT_EQ(ConfigTypeTraits<std::vector<double>>::type, ConfigItemType::DoubleArray);
  EXPECT_EQ(ConfigTypeTraits<std::vector<std::string>>::type, ConfigItemType::StringArray);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(ConfigManagerTest, FullConfigCycle) {
  ConfigManager manager("test");
  auto* root = manager.root_group();

  // Create database config group
  auto db_group = std::make_unique<ConfigGroup>("database", "Database settings");
  db_group->add_item(
      ConfigItem<std::string>::Builder("host", "Database host").default_value("localhost").build());
  db_group->add_item(ConfigItem<int>::Builder("port", "Database port")
                         .default_value(5432)
                         .validator([](const int& v) { return v > 0 && v < 65536; })
                         .build());

  // Create server config group
  auto server_group = std::make_unique<ConfigGroup>("server", "Server settings");
  server_group->add_item(
      ConfigItem<int>::Builder("port", "Server port").default_value(8080).build());
  server_group->add_item(
      ConfigItem<std::vector<std::string>>::Builder("allowed_hosts", "Allowed hosts")
          .default_value(std::vector<std::string>{"localhost", "127.0.0.1"})
          .build());

  // Add groups to root
  root->add_group(std::move(db_group));
  root->add_group(std::move(server_group));

  // Save config
  EXPECT_TRUE(manager.save_to_json("test_configs/full_config.json"));

  // Create new manager and load
  ConfigManager manager2("test2");
  EXPECT_TRUE(manager2.load_from_json("test_configs/full_config.json"));

  // Verify loaded values
  auto* db_host = manager2.find_item<std::string>("database.host");
  ASSERT_NE(db_host, nullptr);
  EXPECT_EQ(db_host->value(), "localhost");

  auto* db_port = manager2.find_item<int>("database.port");
  ASSERT_NE(db_port, nullptr);
  EXPECT_EQ(db_port->value(), 5432);

  auto* server_port = manager2.find_item<int>("server.port");
  ASSERT_NE(server_port, nullptr);
  EXPECT_EQ(server_port->value(), 8080);

  auto* allowed_hosts = manager2.find_item<std::vector<std::string>>("server.allowed_hosts");
  ASSERT_NE(allowed_hosts, nullptr);
  EXPECT_EQ(allowed_hosts->value().size(), 2);
}

TEST_F(ConfigManagerTest, ConfigManagerValidation) {
  ConfigManager manager("test");
  auto* root = manager.root_group();

  root->add_item(ConfigItem<int>::Builder("required1", "Required 1").required(true).build());
  root->add_item(ConfigItem<int>::Builder("required2", "Required 2").required(true).build());
  root->add_item(ConfigItem<int>::Builder("optional", "Optional").default_value(10).build());

  EXPECT_FALSE(manager.validate());

  auto errors = manager.validation_errors();
  EXPECT_EQ(errors.size(), 2);

  // Set one required value
  auto* req1 = manager.find_item<int>("required1");
  ASSERT_NE(req1, nullptr);
  req1->set(100);

  EXPECT_FALSE(manager.validate());
  errors = manager.validation_errors();
  EXPECT_EQ(errors.size(), 1);

  // Set second required value
  auto* req2 = manager.find_item<int>("required2");
  ASSERT_NE(req2, nullptr);
  req2->set(200);

  EXPECT_TRUE(manager.validate());
  errors = manager.validation_errors();
  EXPECT_TRUE(errors.empty());
}

TEST_F(ConfigManagerTest, ConfigManagerSetConfigFile) {
  ConfigManager manager("test");

  manager.set_config_file("test_configs/my_config.json");

  EXPECT_EQ(manager.config_file(), "test_configs/my_config.json");
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_F(ConfigManagerTest, ConfigItemUnset) {
  auto item = ConfigItem<int>::Builder("test", "Test").build();

  EXPECT_FALSE(item->is_set());
  EXPECT_THROW(item->value(), std::runtime_error);

  auto opt_value = item->get();
  EXPECT_FALSE(opt_value.has_value());

  auto or_value = item->get_or(999);
  EXPECT_EQ(or_value, 999);
}

TEST_F(ConfigManagerTest, ConfigManagerInvalidJson) {
  ConfigManager manager("test");

  EXPECT_FALSE(manager.load_from_json_string("{ invalid json }"));
}

TEST_F(ConfigManagerTest, ConfigManagerNonExistentFile) {
  ConfigManager manager("test");

  EXPECT_FALSE(manager.load_from_json("test_configs/nonexistent.json"));
}

TEST_F(ConfigManagerTest, ConfigManagerYamlNotImplemented) {
  ConfigManager manager("test");

  // YAML loader is not implemented in Phase 1
  EXPECT_FALSE(manager.load_from_yaml("test_configs/config.yaml"));
}
