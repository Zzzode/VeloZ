#include "kj/test.h"
#include "veloz/core/json.h"

#include <algorithm>
#include <kj/string.h>

using namespace veloz::core;

namespace {

// Test basic JSON parsing
KJ_TEST("JSON: Parse simple object") {
  kj::StringPtr json_str = R"({
        "name": "test",
        "value": 42,
        "flag": true
    })"_kj;

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  KJ_EXPECT(root.is_valid());
  KJ_EXPECT(root.is_object());

  KJ_IF_SOME(name, root.get("name")) {
    KJ_EXPECT(name.get_string() == "test");
  }
  else {
    KJ_FAIL_EXPECT("name not found");
  }
  KJ_IF_SOME(value, root.get("value")) {
    KJ_EXPECT(value.get_int() == 42);
  }
  else {
    KJ_FAIL_EXPECT("value not found");
  }
  KJ_IF_SOME(flag, root.get("flag")) {
    KJ_EXPECT(flag.get_bool());
  }
  else {
    KJ_FAIL_EXPECT("flag not found");
  }
}

// Test JSON array parsing
KJ_TEST("JSON: Parse array") {
  kj::StringPtr json_str = R"([1, 2, 3, 4, 5])"_kj;

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  KJ_EXPECT(root.is_valid());
  KJ_EXPECT(root.is_array());
  KJ_EXPECT(root.size() == 5);
}

// Test nested objects
KJ_TEST("JSON: Parse nested object") {
  kj::StringPtr json_str = R"({
        "user": {
            "name": "Alice",
            "age": 30,
            "address": {
                "city": "NYC",
                "zip": 10001
            }
        }
    })"_kj;

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  KJ_IF_SOME(user, root.get("user"_kj)) {
    KJ_EXPECT(user.is_object());
  }
  else {
    KJ_FAIL_EXPECT("user not found");
  }
  KJ_EXPECT(root["user"_kj]["name"_kj].get_string() == "Alice"_kj);
  KJ_EXPECT(root["user"_kj]["age"_kj].get_int() == 30);
  KJ_EXPECT(root["user"_kj]["address"_kj]["city"_kj].get_string() == "NYC"_kj);
}

// Test JSON building
KJ_TEST("JSON: Build simple object") {
  auto builder = JsonBuilder::object();
  builder.put("name"_kj, "test"_kj)
      .put("value"_kj, 42)
      .put("flag"_kj, true)
      .put("null_val"_kj, nullptr);

  kj::String json = builder.build();
  auto doc = JsonDocument::parse(json);

  KJ_EXPECT(doc.root()["name"_kj].get_string() == "test"_kj);
  KJ_EXPECT(doc.root()["value"_kj].get_int() == 42);
  KJ_EXPECT(doc.root()["flag"_kj].get_bool());
  KJ_EXPECT(doc.root()["null_val"_kj].is_null());
}

// Test JSON array building
KJ_TEST("JSON: Build array") {
  auto builder = JsonBuilder::array();
  builder.add(1).add(2).add(3).add("string"_kj).add(true);

  kj::String json = builder.build();
  auto doc = JsonDocument::parse(json);

  KJ_EXPECT(doc.root().is_array());
  KJ_EXPECT(doc.root().size() == 5);
  KJ_EXPECT(doc.root()[size_t(0)].get_int() == 1);
  KJ_EXPECT(doc.root()[size_t(3)].get_string() == "string"_kj);
}

// Test nested building
KJ_TEST("JSON: Build nested structure") {
  auto builder = JsonBuilder::object();
  builder.put("name"_kj, "test"_kj)
      .put_object("nested"_kj,
                  [](JsonBuilder& b) { b.put("inner"_kj, "value"_kj).put("number"_kj, 123); })
      .put_array("items"_kj, [](JsonBuilder& b) { b.add(1).add(2).add(3); });

  kj::String json = builder.build(true);
  KJ_EXPECT(json.size() > 0);

  auto doc = JsonDocument::parse(json);

  KJ_EXPECT(doc.root()["nested"_kj]["inner"_kj].get_string() == "value"_kj);
  KJ_EXPECT(doc.root()["nested"_kj]["number"_kj].get_int() == 123);
  KJ_EXPECT(doc.root()["items"_kj].size() == 3);
}

// Test type checking
KJ_TEST("JSON: Type checking") {
  kj::StringPtr json_str = R"({
        "bool_val": true,
        "int_val": 42,
        "double_val": 3.14,
        "string_val": "hello",
        "null_val": null,
        "array_val": [1, 2, 3],
        "object_val": {"key": "value"}
    })"_kj;

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  KJ_EXPECT(root["bool_val"_kj].is_bool());
  KJ_EXPECT(root["int_val"_kj].is_int());
  KJ_EXPECT(root["double_val"_kj].is_real());
  KJ_EXPECT(root["string_val"_kj].is_string());
  KJ_EXPECT(root["null_val"_kj].is_null());
  KJ_EXPECT(root["array_val"_kj].is_array());
  KJ_EXPECT(root["object_val"_kj].is_object());
}

// Test keys extraction
KJ_TEST("JSON: Keys extraction") {
  kj::StringPtr json_str = R"({
        "key1": "value1",
        "key2": "value2",
        "key3": "value3"
    })"_kj;

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  auto keys = root.keys();
  KJ_EXPECT(keys.size() == 3);

  bool saw1 = false;
  bool saw2 = false;
  bool saw3 = false;
  for (auto& k : keys) {
    kj::StringPtr kp = k;
    if (kp == "key1"_kj) {
      saw1 = true;
    } else if (kp == "key2"_kj) {
      saw2 = true;
    } else if (kp == "key3"_kj) {
      saw3 = true;
    }
  }
  KJ_EXPECT(saw1);
  KJ_EXPECT(saw2);
  KJ_EXPECT(saw3);
}

// Test for_each_object
KJ_TEST("JSON: For each object") {
  kj::StringPtr json_str = R"({
        "a": 1,
        "b": 2,
        "c": 3
    })"_kj;

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  int sum = 0;
  root.for_each_object([&sum](kj::StringPtr, const JsonValue& val) { sum += val.get_int(); });
  KJ_EXPECT(sum == 6);
}

KJ_TEST("JSON: String ptr access") {
  kj::StringPtr json_str = R"({"text": "Hello, World!"})"_kj;

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  KJ_IF_SOME(sv, root["text"_kj].get_string_ptr()) {
    KJ_EXPECT(sv == "Hello, World!"_kj);
  }
  else {
    KJ_FAIL_EXPECT("text not found");
  }
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
  // Parse errors throw kj::Exception
  bool threw = false;
  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               auto doc = JsonDocument::parse("{ invalid json }");
               (void)doc;
             })) {
    (void)exception;
    threw = true;
  }
  KJ_EXPECT(threw);
}

// Test JSON copying
KJ_TEST("JSON: Copy values") {
  kj::StringPtr json_str = R"({"key1": "value1", "key2": "value2"})"_kj;

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  auto val1 = root["key1"_kj];
  auto val2 = val1; // Test copy assignment

  KJ_EXPECT(val2.get_string() == "value1"_kj);
}

// Test JSON null handling
KJ_TEST("JSON: Null handling") {
  kj::StringPtr json_str = R"({"null_key": null, "string_key": "value"})"_kj;

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  KJ_EXPECT(root["null_key"_kj].is_null());
  KJ_EXPECT(!root["string_key"_kj].is_null());
  KJ_EXPECT(root["string_key"_kj].is_string());
}

// Test JSON pretty printing
KJ_TEST("JSON: Pretty printing") {
  auto builder = JsonBuilder::object();
  builder.put("a"_kj, 1).put("b"_kj, 2).put("c"_kj, 3);

  auto json = builder.build(true);
  kj::StringPtr jp = json;
  bool hasNewline = false;
  bool hasDoubleSpace = false;
  for (size_t i = 0; i < jp.size(); ++i) {
    if (jp[i] == '\n') {
      hasNewline = true;
    }
    if (i + 1 < jp.size() && jp[i] == ' ' && jp[i + 1] == ' ') {
      hasDoubleSpace = true;
    }
  }
  KJ_EXPECT(hasNewline);
  KJ_EXPECT(hasDoubleSpace);
}

// Test JSON array iteration
KJ_TEST("JSON: Array iteration") {
  kj::StringPtr json_str = R"([1, 2, 3, 4])"_kj;

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  int index = 0;
  root.for_each_array([&index](const JsonValue& val) { KJ_EXPECT(val.get_int() == ++index); });

  KJ_EXPECT(index == 4);
}

// Test JSON object iteration
KJ_TEST("JSON: Object iteration") {
  kj::StringPtr json_str = R"({"a": 1, "b": 2, "c": 3})"_kj;

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  int count = 0;
  root.for_each_object([&count](kj::StringPtr, const JsonValue&) { ++count; });

  KJ_EXPECT(count == 3);
}

// Test JSON value equality
KJ_TEST("JSON: Value equality") {
  auto doc = JsonDocument::parse(R"({"value": 42})");

  auto root = doc.root();

  auto val1_opt = root.get("value");
  auto val2_opt = root.get("value");

  KJ_IF_SOME(val1, val1_opt) {
    KJ_EXPECT(val1.get_int() == 42);
  }
  else {
    KJ_FAIL_EXPECT("val1 not found");
  }
  KJ_IF_SOME(val2, val2_opt) {
    KJ_EXPECT(val2.get_int() == 42);
  }
  else {
    KJ_FAIL_EXPECT("val2 not found");
  }
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
  kj::StringPtr json_str = R"({"arrays": [[1, 2], [3, 4]]})"_kj;

  auto doc = JsonDocument::parse(json_str);
  auto root = doc.root();

  KJ_EXPECT(root["arrays"_kj].is_array());
  KJ_EXPECT(root["arrays"_kj].size() == 2);
  KJ_EXPECT(root["arrays"_kj][0].is_array());
  KJ_EXPECT(root["arrays"_kj][0].size() == 2);
}

} // namespace
