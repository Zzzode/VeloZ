#include <gtest/gtest.h>
#include "veloz/core/json.h"
#include <algorithm>

namespace veloz::core::tests {

class JsonTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test basic JSON parsing
TEST_F(JsonTest, ParseSimpleObject) {
    std::string json_str = R"({
        "name": "test",
        "value": 42,
        "flag": true
    })";

    auto doc = JsonDocument::parse(json_str);
    auto root = doc.root();

    EXPECT_TRUE(root.is_valid());
    EXPECT_TRUE(root.is_object());

    EXPECT_EQ(root.get("name").value().get_string(), "test");
    EXPECT_EQ(root.get("value").value().get_int(), 42);
    EXPECT_TRUE(root.get("flag").value().get_bool());
}

// Test JSON array parsing
TEST_F(JsonTest, ParseArray) {
    std::string json_str = R"([1, 2, 3, 4, 5])";

    auto doc = JsonDocument::parse(json_str);
    auto root = doc.root();

    EXPECT_TRUE(root.is_valid());
    EXPECT_TRUE(root.is_array());
    EXPECT_EQ(root.size(), 5);

    int sum = 0;
    root.for_each_array([&sum](const JsonValue& val) {
        sum += val.get_int();
    });

    EXPECT_EQ(sum, 15);
}

// Test nested objects
TEST_F(JsonTest, ParseNestedObject) {
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

    EXPECT_TRUE(root.get("user").value().is_object());
    EXPECT_EQ(root["user"]["name"].get_string(), "Alice");
    EXPECT_EQ(root["user"]["age"].get_int(), 30);
    EXPECT_EQ(root["user"]["address"]["city"].get_string(), "NYC");
}

// Test JSON building
TEST_F(JsonTest, BuildSimpleObject) {
    auto builder = JsonBuilder::object();
    builder.put("name", "test")
            .put("value", 42)
            .put("flag", true)
            .put("null_val", nullptr);

    std::string json = builder.build();
    auto doc = JsonDocument::parse(json);

    EXPECT_EQ(doc.root()["name"].get_string(), "test");
    EXPECT_EQ(doc.root()["value"].get_int(), 42);
    EXPECT_TRUE(doc.root()["flag"].get_bool());
    EXPECT_TRUE(doc.root()["null_val"].is_null());
}

// Test JSON array building
TEST_F(JsonTest, BuildArray) {
    auto builder = JsonBuilder::array();
    builder.add(1).add(2).add(3).add("string").add(true);

    std::string json = builder.build();
    auto doc = JsonDocument::parse(json);

    EXPECT_TRUE(doc.root().is_array());
    EXPECT_EQ(doc.root().size(), 5);
    EXPECT_EQ(doc.root()[size_t(0)].get_int(), 1);
    EXPECT_EQ(doc.root()[size_t(3)].get_string(), "string");
}

// Test nested building
TEST_F(JsonTest, BuildNestedStructure) {
    auto builder = JsonBuilder::object();
    builder.put("name", "test")
            .put_object("nested", [](JsonBuilder& b) {
                b.put("inner", "value")
                 .put("number", 123);
            })
            .put_array("items", [](JsonBuilder& b) {
                b.add(1).add(2).add(3);
            });

    std::string json = builder.build(true);  // pretty print
    auto doc = JsonDocument::parse(json);

    EXPECT_EQ(doc.root()["nested"]["inner"].get_string(), "value");
    EXPECT_EQ(doc.root()["nested"]["number"].get_int(), 123);
    EXPECT_EQ(doc.root()["items"].size(), 3);
}

// Test optional values
TEST_F(JsonTest, OptionalValues) {
    std::string json_str = R"({"exists": "value"})";

    auto doc = JsonDocument::parse(json_str);
    auto root = doc.root();

    auto exists = root.get("exists");
    auto missing = root.get("missing");

    EXPECT_TRUE(exists.has_value());
    EXPECT_FALSE(missing.has_value());

    EXPECT_EQ(exists.value().get_string(), "value");
    EXPECT_EQ(missing.value_or(JsonValue(nullptr)).get_string("default"), "default");
}

// Test type checking
TEST_F(JsonTest, TypeChecking) {
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

    EXPECT_TRUE(root["bool_val"].is_bool());
    EXPECT_TRUE(root["int_val"].is_int());
    EXPECT_TRUE(root["double_val"].is_real());
    EXPECT_TRUE(root["string_val"].is_string());
    EXPECT_TRUE(root["null_val"].is_null());
    EXPECT_TRUE(root["array_val"].is_array());
    EXPECT_TRUE(root["object_val"].is_object());
}

// Test default values
TEST_F(JsonTest, DefaultValues) {
    std::string json_str = R"({"valid": 123})";

    auto doc = JsonDocument::parse(json_str);
    auto root = doc.root();

    // Missing key returns default
    EXPECT_EQ(root.get("missing").value().get_int(999), 999);
    EXPECT_EQ(root.get("missing").value().get_string("default"), "default");
    EXPECT_EQ(root.get("missing").value().get_bool(true), true);

    // Wrong type also returns default
    EXPECT_EQ(root["valid"].get_string("default"), "default");
    EXPECT_EQ(root["valid"].get_bool(true), true);
}

// Test keys extraction
TEST_F(JsonTest, KeysExtraction) {
    std::string json_str = R"({
        "key1": "value1",
        "key2": "value2",
        "key3": "value3"
    })";

    auto doc = JsonDocument::parse(json_str);
    auto root = doc.root();

    auto keys = root.keys();
    EXPECT_EQ(keys.size(), 3);

    std::sort(keys.begin(), keys.end());
    EXPECT_EQ(keys[0], "key1");
    EXPECT_EQ(keys[1], "key2");
    EXPECT_EQ(keys[2], "key3");
}

// Test for_each_object
TEST_F(JsonTest, ForEachObject) {
    std::string json_str = R"({
        "a": 1,
        "b": 2,
        "c": 3
    })";

    auto doc = JsonDocument::parse(json_str);
    auto root = doc.root();

    int sum = 0;
    root.for_each_object([&sum](const std::string&, const JsonValue& val) {
        sum += val.get_int();
    });

    EXPECT_EQ(sum, 6);
}

// Test string_view for zero-copy access
TEST_F(JsonTest, StringViewAccess) {
    std::string json_str = R"({"text": "Hello, World!"})";

    auto doc = JsonDocument::parse(json_str);
    auto root = doc.root();

    std::string_view sv = root["text"].get_string_view();
    EXPECT_EQ(sv, "Hello, World!");
}

// Test numeric conversions
TEST_F(JsonTest, NumericConversions) {
    std::string json_str = R"({
        "large_uint": 4294967295,
        "negative_int": -12345,
        "float_val": 3.14159
    })";

    auto doc = JsonDocument::parse(json_str);
    auto root = doc.root();

    EXPECT_EQ(root["large_uint"].get_uint(), 4294967295ULL);
    EXPECT_EQ(root["negative_int"].get_int(), -12345);
    EXPECT_NEAR(root["float_val"].get_double(), 3.14159, 0.00001);
}

// Test JSON utilities
TEST_F(JsonTest, JsonValidation) {
    EXPECT_TRUE(json_utils::is_valid_json(R"({"key": "value"})"));
    EXPECT_TRUE(json_utils::is_valid_json(R"([1, 2, 3])"));
    EXPECT_FALSE(json_utils::is_valid_json(R"({"invalid": })"));
    EXPECT_FALSE(json_utils::is_valid_json("not json"));
}

} // namespace veloz::core::tests

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
