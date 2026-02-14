#include "kj/test.h"
#include "veloz/core/json.h"

#include <algorithm>
#include <kj/string.h>
#include <string> // Kept for JsonDocument API compatibility

using namespace veloz::core;

namespace {

// Test basic JSON parsing
KJ_TEST("JSON: Parse simple object") {
  std::string json_str = R"({
        "name": "test",
        "value": 42,
        "flag": true
    })";

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  KJ_EXPECT(root.is_valid());
  KJ_EXPECT(root.is_object());

  KJ_EXPECT(root.get("name").value().get_string() == "test");
  KJ_EXPECT(root.get("value").value().get_int() == 42);
  KJ_EXPECT(root.get("flag").value().get_bool());
}

// Test JSON array parsing
KJ_TEST("JSON: Parse array") {
  std::string json_str = R"([1, 2, 3, 4, 5])";

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  KJ_EXPECT(root.is_valid());
  KJ_EXPECT(root.is_array());
  KJ_EXPECT(root.size() == 5);
}

// Test nested objects
KJ_TEST("JSON: Parse nested object") {
  std::string json_str = R"({
        "user": {
            "name": "Alice",
            "age": 30,
            "address": {
                "city": "NYC",
                "zip": 10001
            }
        }
    })";

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  KJ_EXPECT(root.get("user").value().is_object());
  KJ_EXPECT(root["user"]["name"].get_string() == "Alice");
  KJ_EXPECT(root["user"]["age"].get_int() == 30);
  KJ_EXPECT(root["user"]["address"]["city"].get_string() == "NYC");
}

// Test JSON building
KJ_TEST("JSON: Build simple object") {
  auto builder = JsonBuilder::object();
  builder.put("name", "test").put("value", 42).put("flag", true).put("null_val", nullptr);

  std::string json = builder.build();
  auto doc = JsonDocument::parse(json);

  KJ_EXPECT(doc.root()["name"].get_string() == "test");
  KJ_EXPECT(doc.root()["value"].get_int() == 42);
  KJ_EXPECT(doc.root()["flag"].get_bool());
  KJ_EXPECT(doc.root()["null_val"].is_null());
}

// Test JSON array building
KJ_TEST("JSON: Build array") {
  auto builder = JsonBuilder::array();
  builder.add(1).add(2).add(3).add("string").add(true);

  std::string json = builder.build();
  auto doc = JsonDocument::parse(json);

  KJ_EXPECT(doc.root().is_array());
  KJ_EXPECT(doc.root().size() == 5);
  KJ_EXPECT(doc.root()[size_t(0)].get_int() == 1);
  KJ_EXPECT(doc.root()[size_t(3)].get_string() == "string");
}

// Test nested building
// TODO: Fix nested structure building - yyjson integration issue
// KJ_TEST("JSON: Build nested structure") {
//   auto builder = JsonBuilder::object();
//   builder.put("name", "test")
//       .put_object("nested", [](JsonBuilder& b) { b.put("inner", "value").put("number", 123); })
//       .put_array("items", [](JsonBuilder& b) { b.add(1).add(2).add(3); });
//
//   std::string json = builder.build(true); // pretty print
//   KJ_EXPECT(!json.empty()); // Check that build() returns non-empty string
//
//   auto doc = JsonDocument::parse(json);
//
//   KJ_EXPECT(doc.root()["nested"]["inner"].get_string() == "value");
//   KJ_EXPECT(doc.root()["nested"]["number"].get_int() == 123);
//   KJ_EXPECT(doc.root()["items"].size() == 3);
// }

// Test type checking
KJ_TEST("JSON: Type checking") {
  std::string json_str = R"({
        "bool_val": true,
        "int_val": 42,
        "double_val": 3.14,
        "string_val": "hello",
        "null_val": null,
        "array_val": [1, 2, 3],
        "object_val": {"key": "value"}
    })";

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  KJ_EXPECT(root["bool_val"].is_bool());
  KJ_EXPECT(root["int_val"].is_int());
  KJ_EXPECT(root["double_val"].is_real());
  KJ_EXPECT(root["string_val"].is_string());
  KJ_EXPECT(root["null_val"].is_null());
  KJ_EXPECT(root["array_val"].is_array());
  KJ_EXPECT(root["object_val"].is_object());
}

// Test keys extraction
KJ_TEST("JSON: Keys extraction") {
  std::string json_str = R"({
        "key1": "value1",
        "key2": "value2",
        "key3": "value3"
    })";

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  auto keys = root.keys();
  KJ_EXPECT(keys.size() == 3);

  std::sort(keys.begin(), keys.end());
  KJ_EXPECT(keys[0] == "key1");
  KJ_EXPECT(keys[1] == "key2");
  KJ_EXPECT(keys[2] == "key3");
}

// Test for_each_object
KJ_TEST("JSON: For each object") {
  std::string json_str = R"({
        "a": 1,
        "b": 2,
        "c": 3
    })";

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  int sum = 0;
  root.for_each_object([&sum](const std::string&, const JsonValue& val) { sum += val.get_int(); });
  KJ_EXPECT(sum == 6);
}

// Test string_view for zero-copy access
KJ_TEST("JSON: String view access") {
  std::string json_str = R"({"text": "Hello, World!"})";

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  std::string_view sv = root["text"].get_string_view();
  KJ_EXPECT(sv == "Hello, World!");
}

// Test JSON utilities
KJ_TEST("JSON: Validation") {
  KJ_EXPECT(json_utils::is_valid_json(R"({"key": "value"})"));
  KJ_EXPECT(json_utils::is_valid_json(R"([1, 2, 3])"));
  KJ_EXPECT(!json_utils::is_valid_json(R"({"invalid": })"));
  KJ_EXPECT(!json_utils::is_valid_json("not json"));
}

// Test JSON errors
KJ_TEST("JSON: Parse error handling") {
  // Parse errors throw exceptions
  bool threw = false;
  try {
    auto doc = JsonDocument::parse("{ invalid json }");
  } catch (const std::runtime_error&) {
    threw = true;
  }
  KJ_EXPECT(threw);
}

// Test JSON copying
KJ_TEST("JSON: Copy values") {
  std::string json_str = R"({"key1": "value1", "key2": "value2"})";

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  auto val1 = root["key1"];
  auto val2 = val1; // Test copy assignment

  KJ_EXPECT(val2.get_string() == "value1");
}

// Test JSON null handling
KJ_TEST("JSON: Null handling") {
  std::string json_str = R"({"null_key": null, "string_key": "value"})";

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  KJ_EXPECT(root["null_key"].is_null());
  KJ_EXPECT(!root["string_key"].is_null());
  KJ_EXPECT(root["string_key"].is_string());
}

// Test JSON pretty printing
KJ_TEST("JSON: Pretty printing") {
  auto builder = JsonBuilder::object();
  builder.put("a", 1).put("b", 2).put("c", 3);

  std::string json = builder.build(true); // pretty
  KJ_EXPECT(json.find("\n") != std::string::npos);
  KJ_EXPECT(json.find("  ") != std::string::npos);
}

// Test JSON array iteration
KJ_TEST("JSON: Array iteration") {
  std::string json_str = R"([1, 2, 3, 4])";

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  int index = 0;
  root.for_each_array([&index](const JsonValue& val) { KJ_EXPECT(val.get_int() == ++index); });

  KJ_EXPECT(index == 4);
}

// Test JSON object iteration
KJ_TEST("JSON: Object iteration") {
  std::string json_str = R"({"a": 1, "b": 2, "c": 3})";

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  int count = 0;
  root.for_each_object([&count](const std::string&, const JsonValue&) { ++count; });

  KJ_EXPECT(count == 3);
}

// Test JSON value equality
KJ_TEST("JSON: Value equality") {
  auto doc = JsonDocument::parse(R"({"value": 42})");

  auto root = doc.root();

  auto val1_opt = root.get("value");
  auto val2_opt = root.get("value");

  KJ_EXPECT(val1_opt.has_value());
  KJ_EXPECT(val2_opt.has_value());
  KJ_EXPECT(val1_opt->get_int() == 42);
  KJ_EXPECT(val2_opt->get_int() == 42);
}

// Test JSON empty handling
KJ_TEST("JSON: Empty handling") {
  auto obj_doc = JsonDocument::parse(R"({})");
  KJ_EXPECT(obj_doc.root().is_object());
  KJ_EXPECT(obj_doc.root().size() == 0);

  auto arr_doc = JsonDocument::parse(R"([])");
  KJ_EXPECT(arr_doc.root().is_array());
  KJ_EXPECT(arr_doc.root().size() == 0);
}

// Test JSON nested arrays
KJ_TEST("JSON: Nested arrays") {
  std::string json_str = R"({"arrays": [[1, 2], [3, 4]]})";

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  KJ_EXPECT(root["arrays"].is_array());
  KJ_EXPECT(root["arrays"].size() == 2);
  KJ_EXPECT(root["arrays"][0].is_array());
  KJ_EXPECT(root["arrays"][0].size() == 2);
}

} // namespace
