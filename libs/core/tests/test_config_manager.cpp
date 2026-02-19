#include "kj/test.h"
#include "veloz/core/config_manager.h"

#include <fstream>
#include <kj/filesystem.h> // kj::Path and kj::newDiskFilesystem for filesystem operations
#include <kj/memory.h>
#include <kj/string.h>
#include <string> // Kept for ConfigItem<std::string> template parameter

using namespace veloz::core;

namespace {

// ============================================================================
// ConfigItem Tests
// ============================================================================

KJ_TEST("ConfigItem: Builder") {
  auto item =
      ConfigItem<int>::Builder("test_item", "Test item").default_value(42).required(false).build();

  KJ_EXPECT(item->key() == "test_item"_kj);
  KJ_EXPECT(item->description() == "Test item"_kj);
  KJ_EXPECT(!item->is_required());
  KJ_EXPECT(item->has_default());
  KJ_EXPECT(item->is_set());
}

KJ_TEST("ConfigItem: Get value") {
  auto item = ConfigItem<int>::Builder("test", "Test").default_value(100).build();

  auto maybe_value = item->get();
  KJ_IF_SOME(value, maybe_value) {
    KJ_EXPECT(value == 100);
  }
  else {
    KJ_FAIL_EXPECT("value not found");
  }
  KJ_EXPECT(item->value() == 100);
}

KJ_TEST("ConfigItem: Set value") {
  auto item = ConfigItem<int>::Builder("test", "Test").default_value(10).build();

  KJ_EXPECT(item->set(200));
  KJ_EXPECT(item->value() == 200);
}

KJ_TEST("ConfigItem: Set value with validation failure") {
  auto item = ConfigItem<int>::Builder("test", "Test")
                  .validator([](const int& v) { return v >= 0 && v <= 100; })
                  .build();

  KJ_EXPECT(item->set(50));
  KJ_EXPECT(item->value() == 50);

  KJ_EXPECT(!item->set(150));     // Should fail validation
  KJ_EXPECT(item->value() == 50); // Value unchanged
}

KJ_TEST("ConfigItem: Reset") {
  auto item = ConfigItem<int>::Builder("test", "Test").default_value(100).build();
  item->set(200);

  item->reset();
  KJ_EXPECT(item->value() == 100);
}

KJ_TEST("ConfigItem: To string") {
  auto int_item = ConfigItem<int>::Builder("int", "Int").default_value(42).build();
  KJ_EXPECT(int_item->to_string() == "42");

  auto string_item = ConfigItem<std::string>::Builder("str", "Str").default_value("hello").build();
  KJ_EXPECT(string_item->to_string() == "\"hello\"");

  auto bool_item = ConfigItem<bool>::Builder("bool", "Bool").default_value(true).build();
  KJ_EXPECT(bool_item->to_string() == "true");

  KJ_EXPECT(bool_item->from_string("42") == false);
  KJ_EXPECT(bool_item->from_string("true") == true);
  KJ_EXPECT(bool_item->value());

  KJ_EXPECT(bool_item->from_string("false") == true);
  KJ_EXPECT(!bool_item->value());
}

KJ_TEST("ConfigItem: Array") {
  auto array_item = ConfigItem<std::vector<int>>::Builder("array", "Array")
                        .default_value(std::vector<int>{1, 2, 3})
                        .build();

  auto maybe_value = array_item->get();
  KJ_IF_SOME(value, maybe_value) {
    KJ_EXPECT(value.size() == 3);
    KJ_EXPECT(value[0] == 1);
    KJ_EXPECT(value[1] == 2);
    KJ_EXPECT(value[2] == 3);
  }
  else {
    KJ_FAIL_EXPECT("array value not found");
  }

  kj::String array_str = array_item->to_string();
  // Convert kj::String to std::string_view for find() operation
  std::string_view array_sv(array_str.cStr(), array_str.size());
  KJ_EXPECT(array_sv.find("[") != std::string_view::npos);
}

KJ_TEST("ConfigItem: Callback") {
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

  item->set(20);
  KJ_EXPECT(callback_count == 1);
  KJ_EXPECT(old_value == 10);
  KJ_EXPECT(new_value == 20);

  item->set(30);
  KJ_EXPECT(callback_count == 2);
  KJ_EXPECT(old_value == 20);
  KJ_EXPECT(new_value == 30);

  item->set(100);
  KJ_EXPECT(callback_count == 3);
  KJ_EXPECT(old_value == 30);
  KJ_EXPECT(new_value == 100);
}

// ============================================================================
// ConfigGroup Tests
// ============================================================================

KJ_TEST("ConfigGroup: Add item") {
  ConfigGroup group("test_group", "Test group");

  auto item1 = ConfigItem<int>::Builder("item1", "Item 1").default_value(1).build();
  auto item2 = ConfigItem<std::string>::Builder("item2", "Item 2").default_value("test").build();

  group.add_item(kj::mv(item1));
  group.add_item(kj::mv(item2));

  auto* found_item1 = group.get_item<int>("item1");
  KJ_EXPECT(found_item1 != nullptr);
  KJ_EXPECT(found_item1->value() == 1);

  auto* found_item2 = group.get_item<std::string>("item2");
  KJ_EXPECT(found_item2 != nullptr);
  KJ_EXPECT(found_item2->value() == "test");
}

KJ_TEST("ConfigGroup: Subgroups") {
  ConfigGroup root("root", "Root group");
  // Uses std::make_unique for polymorphic ownership pattern (kj::Own lacks release())
  auto sub1 = kj::heap<ConfigGroup>("sub1", "Subgroup 1");
  auto sub2 = kj::heap<ConfigGroup>("sub2", "Subgroup 2");

  root.add_group(kj::mv(sub1));
  root.add_group(kj::mv(sub2));

  auto* found_sub1 = root.get_group("sub1");
  KJ_EXPECT(found_sub1 != nullptr);
  KJ_EXPECT(found_sub1->name() == "sub1");

  auto* found_sub2 = root.get_group("sub2");
  KJ_EXPECT(found_sub2 != nullptr);
  KJ_EXPECT(found_sub2->name() == "sub2");
}

KJ_TEST("ConfigGroup: Validate") {
  ConfigGroup group("test", "Test");

  auto optional_item = ConfigItem<int>::Builder("opt", "Optional").default_value(1).build();
  auto required_item = ConfigItem<int>::Builder("req", "Required").required(true).build();

  group.add_item(kj::mv(optional_item));
  group.add_item(kj::mv(required_item));

  KJ_EXPECT(!group.validate()); // Required item not set
  KJ_EXPECT(group.validation_errors().size() > 0);

  // Get the item from the group (not from the moved unique_ptr)
  auto* req_item_ptr = group.get_item<int>("req");
  KJ_EXPECT(req_item_ptr != nullptr);
  req_item_ptr->set(10);

  KJ_EXPECT(group.validate());
  KJ_EXPECT(group.validation_errors().empty());
}

KJ_TEST("ConfigGroup: Get items") {
  ConfigGroup group("test", "Test");

  auto item1 = ConfigItem<int>::Builder("item1", "1").default_value(1).build();
  auto item2 = ConfigItem<std::string>::Builder("item2", "2").default_value("test").build();

  group.add_item(kj::mv(item1));
  group.add_item(kj::mv(item2));

  auto items = group.get_items();
  KJ_EXPECT(items.size() == 2);
}

// ============================================================================
// ConfigManager Tests
// ============================================================================

KJ_TEST("ConfigManager: Basic") {
  ConfigManager manager("test");
  auto* root = manager.root_group();
  KJ_EXPECT(root != nullptr);
  KJ_EXPECT(root->name() == "root");
}

KJ_TEST("ConfigManager: Load from JSON file") {
  // Create directory using KJ filesystem API
  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getCurrent();
  root.openSubdir(kj::Path{"test_configs"}, kj::WriteMode::CREATE | kj::WriteMode::CREATE_PARENT);

  std::ofstream ofs("test_configs/config.json");
  ofs << R"({
    "timeout": 30,
    "max_connections": 100,
    "enabled": true,
    "server_name": "test-server"
  })";
  ofs.close();

  ConfigManager manager("test");
  KJ_EXPECT(manager.load_from_json(kj::Path::parse("test_configs/config.json")));
}

KJ_TEST("ConfigManager: Load from JSON string") {
  std::string json = R"({
    "value1": 42,
    "value2": "hello",
    "value3": false
  })";

  ConfigManager manager("test");
  KJ_EXPECT(manager.load_from_json_string(json));
}

KJ_TEST("ConfigManager: Find item") {
  ConfigManager manager("test");
  auto* root = manager.root_group();

  root->add_item(ConfigItem<int>::Builder("test_item", "Test").default_value(100).build());

  auto* item = manager.find_item("test_item");
  KJ_EXPECT(item != nullptr);
  KJ_EXPECT(item->key() == "test_item");

  auto* typed_item = manager.find_item<int>("test_item");
  KJ_EXPECT(typed_item != nullptr);
  KJ_EXPECT(typed_item->value() == 100);
}

KJ_TEST("ConfigManager: Save to JSON file") {
  ConfigManager manager("test");
  auto* root = manager.root_group();

  root->add_item(ConfigItem<int>::Builder("int_val", "Int").default_value(42).build());
  root->add_item(ConfigItem<std::string>::Builder("str_val", "Str").default_value("test").build());
  root->add_item(ConfigItem<bool>::Builder("bool_val", "Bool").default_value(true).build());

  KJ_EXPECT(manager.save_to_json(kj::Path::parse("test_configs/saved.json")));

  // Verify file exists using KJ filesystem API
  auto fs = kj::newDiskFilesystem();
  auto& root_dir = fs->getCurrent();
  KJ_EXPECT(root_dir.exists(kj::Path{"test_configs", "saved.json"}));

  // Verify content
  std::ifstream ifs("test_configs/saved.json");
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  KJ_EXPECT(content.find("\"int_val\"") != std::string::npos);
  KJ_EXPECT(content.find("\"str_val\"") != std::string::npos);
  KJ_EXPECT(content.find("\"bool_val\"") != std::string::npos);
}

KJ_TEST("ConfigManager: Nested groups") {
  ConfigManager manager("test");
  auto* root = manager.root_group();

  // Uses std::make_unique for polymorphic ownership pattern (kj::Own lacks release())
  auto db_group = kj::heap<ConfigGroup>("database", "Database config");
  db_group->add_item(
      ConfigItem<std::string>::Builder("host", "Host").default_value("localhost").build());
  db_group->add_item(ConfigItem<int>::Builder("port", "Port").default_value(5432).build());

  root->add_group(kj::mv(db_group));

  auto* host_item = manager.find_item<std::string>("database.host");
  KJ_EXPECT(host_item != nullptr);
  KJ_EXPECT(host_item->value() == "localhost");

  auto* port_item = manager.find_item<int>("database.port");
  KJ_EXPECT(port_item != nullptr);
  KJ_EXPECT(port_item->value() == 5432);
}

KJ_TEST("ConfigManager: Validation") {
  ConfigManager manager("test");

  auto* root = manager.root_group();

  auto req1 = ConfigItem<int>::Builder("required1", "Required 1").required(true).build();
  auto req2 = ConfigItem<int>::Builder("required2", "Required 2").required(true).build();
  auto opt = ConfigItem<int>::Builder("optional", "Optional").default_value(10).build();

  root->add_item(kj::mv(req1));
  root->add_item(kj::mv(req2));
  root->add_item(kj::mv(opt));

  KJ_EXPECT(!manager.validate());
  KJ_EXPECT(manager.validation_errors().size() == 2);

  // Set one required value - validation should still fail
  auto* req1_ptr = manager.find_item<int>("required1");
  req1_ptr->set(100);

  KJ_EXPECT(!manager.validate());
  KJ_EXPECT(manager.validation_errors().size() == 1);

  // Set the second required value - now validation should pass
  auto* req2_ptr = manager.find_item<int>("required2");
  req2_ptr->set(200);

  KJ_EXPECT(manager.validate());
  KJ_EXPECT(manager.validation_errors().empty());
}

KJ_TEST("ConfigManager: Set config file") {
  ConfigManager manager("test");

  manager.set_config_file(kj::Path::parse("test_configs/my_config.json"));
  KJ_IF_SOME(path, manager.config_file()) {
    KJ_EXPECT(path.toString() == "test_configs/my_config.json");
  }
  else {
    KJ_FAIL_EXPECT("config_file should be set");
  }
}

KJ_TEST("ConfigManager: Hot reload") {
  ConfigManager manager("test");

  int callback_count = 0;
  manager.add_hot_reload_callback([&] { ++callback_count; });
  manager.set_hot_reload_enabled(true);
  KJ_EXPECT(manager.hot_reload_enabled());

  manager.trigger_hot_reload();
  KJ_EXPECT(callback_count == 1);

  manager.trigger_hot_reload();
  KJ_EXPECT(callback_count == 2);
}

KJ_TEST("ConfigManager: Full config cycle") {
  ConfigManager manager("test");
  auto* root = manager.root_group();

  // Create database config group
  // Uses std::make_unique for polymorphic ownership pattern (kj::Own lacks release())
  auto db_group = kj::heap<ConfigGroup>("database", "Database settings");
  db_group->add_item(
      ConfigItem<std::string>::Builder("host", "Host").default_value("localhost").build());
  db_group->add_item(ConfigItem<int>::Builder("port", "Port")
                         .default_value(5432)
                         .validator([](const int& v) { return v > 0 && v < 65536; })
                         .build());

  // Create server config group
  auto server_group = kj::heap<ConfigGroup>("server", "Server settings");
  server_group->add_item(
      ConfigItem<int>::Builder("port", "Server port").default_value(8080).build());
  server_group->add_item(
      ConfigItem<std::vector<std::string>>::Builder("allowed_hosts", "Allowed hosts")
          .default_value(std::vector<std::string>{"localhost", "127.0.0.1"})
          .build());

  root->add_group(kj::mv(db_group));
  root->add_group(kj::mv(server_group));

  // Save config
  KJ_EXPECT(manager.save_to_json(kj::Path::parse("test_configs/full_config.json")));

  // Verify file exists using KJ filesystem API
  auto fs = kj::newDiskFilesystem();
  auto& root_dir = fs->getCurrent();
  KJ_EXPECT(root_dir.exists(kj::Path{"test_configs", "full_config.json"}));

  // Create new manager and load
  ConfigManager manager2("test2");
  KJ_EXPECT(manager2.load_from_json(kj::Path::parse("test_configs/full_config.json")));

  // Verify loaded values
  auto* db_host = manager2.find_item<std::string>("database.host");
  KJ_EXPECT(db_host != nullptr);
  KJ_EXPECT(db_host->value() == "localhost");

  auto* db_port = manager2.find_item<int>("database.port");
  KJ_EXPECT(db_port != nullptr);
  KJ_EXPECT(db_port->value() == 5432);

  auto* server_port = manager2.find_item<int>("server.port");
  KJ_EXPECT(server_port != nullptr);
  KJ_EXPECT(server_port->value() == 8080);

  auto* allowed_hosts = manager2.find_item<std::vector<std::string>>("server.allowed_hosts");
  KJ_EXPECT(allowed_hosts != nullptr);
  KJ_EXPECT(allowed_hosts->value().size() == 2);
}

// ============================================================================
// ConfigItemType Tests
// ============================================================================

KJ_TEST("ConfigItemType: Traits") {
  KJ_EXPECT(ConfigTypeTraits<bool>::type == ConfigItemType::Bool);
  KJ_EXPECT(ConfigTypeTraits<int>::type == ConfigItemType::Int);
  KJ_EXPECT(ConfigTypeTraits<int64_t>::type == ConfigItemType::Int64);
  KJ_EXPECT(ConfigTypeTraits<double>::type == ConfigItemType::Double);
  KJ_EXPECT(ConfigTypeTraits<std::string>::type == ConfigItemType::String);
  KJ_EXPECT(ConfigTypeTraits<std::vector<bool>>::type == ConfigItemType::BoolArray);
  KJ_EXPECT(ConfigTypeTraits<std::vector<int>>::type == ConfigItemType::IntArray);
  KJ_EXPECT(ConfigTypeTraits<std::vector<int64_t>>::type == ConfigItemType::Int64Array);
  KJ_EXPECT(ConfigTypeTraits<std::vector<double>>::type == ConfigItemType::DoubleArray);
  KJ_EXPECT(ConfigTypeTraits<std::vector<std::string>>::type == ConfigItemType::StringArray);
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

KJ_TEST("ConfigManager: Unset item") {
  ConfigManager manager("test");
  auto item = ConfigItem<int>::Builder("test", "Test").build();

  // Item not set, should throw kj::Exception
  bool threw = false;
  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() { (void)item->value(); })) {
    threw = true;
  }
  KJ_EXPECT(threw);
}

KJ_TEST("ConfigManager: Invalid JSON") {
  ConfigManager manager("test");

  KJ_EXPECT(!manager.load_from_json_string("{ invalid json }"));
  KJ_EXPECT(!manager.load_from_json(kj::Path::parse("test_configs/nonexistent.json")));
  KJ_EXPECT(!manager.load_from_yaml(kj::Path::parse("test_configs/config.yaml")));
}

// ============================================================================
// Cleanup
// ============================================================================

KJ_TEST("ConfigManager: Cleanup") {
  // Clean up test config files using KJ filesystem API
  auto fs = kj::newDiskFilesystem();
  auto& root_dir = fs->getCurrent();

  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
    root_dir.remove(kj::Path{"test_configs"});
  })) {
    // Directory might not exist or removal failed, ignore
  }

  KJ_EXPECT(!root_dir.exists(kj::Path{"test_configs"}));
}

} // namespace
