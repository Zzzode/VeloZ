#include "veloz/gateway/auth/api_key_manager.h"

#include <atomic>
#include <kj/common.h>
#include <kj/string.h>
#include <kj/test.h>
#include <thread>
#include <vector>

using namespace veloz::gateway;

namespace {

KJ_TEST("ApiKeyManager: Create and validate key") {
  ApiKeyManager manager;

  kj::Vector<kj::String> permissions;
  permissions.add(kj::str("read"));
  permissions.add(kj::str("write"));

  auto pair = manager.create_key("user123"_kj, "test-key"_kj, kj::mv(permissions));

  // Verify key_id and raw_key are non-empty
  KJ_EXPECT(pair.key_id.size() == 16, "Key ID should be 16 characters");
  KJ_EXPECT(pair.raw_key.size() == 64, "Raw key should be 64 characters (32 bytes hex)");

  // Validate the key
  KJ_IF_SOME(validated, manager.validate(pair.raw_key)) {
    KJ_EXPECT(validated->key_id == pair.key_id);
    KJ_EXPECT(validated->user_id == "user123"_kj);
    KJ_EXPECT(validated->name == "test-key"_kj);
    KJ_EXPECT(validated->permissions.size() == 2);
    KJ_EXPECT(ApiKeyManager::has_permission(*validated, "read"_kj));
    KJ_EXPECT(ApiKeyManager::has_permission(*validated, "write"_kj));
    KJ_EXPECT(!validated->revoked);
  }
  else {
    KJ_FAIL_EXPECT("Key validation failed");
  }
}

KJ_TEST("ApiKeyManager: Invalid key returns none") {
  ApiKeyManager manager;

  // Create a key
  kj::Vector<kj::String> permissions;
  auto pair = manager.create_key("user123"_kj, "test-key"_kj, kj::mv(permissions));

  // Try to validate with wrong key
  auto result =
      manager.validate("0000000000000000000000000000000000000000000000000000000000000000"_kj);
  KJ_EXPECT(result == kj::none);
}

KJ_TEST("ApiKeyManager: Revoke key") {
  ApiKeyManager manager;

  kj::Vector<kj::String> permissions;
  auto pair = manager.create_key("user123"_kj, "test-key"_kj, kj::mv(permissions));

  // Validate should work before revocation
  KJ_IF_SOME(validated, manager.validate(pair.raw_key)) {
    (void)validated;
  }
  else {
    KJ_FAIL_EXPECT("Key validation failed before revocation");
  }

  // Revoke the key
  bool revoked = manager.revoke(pair.key_id);
  KJ_EXPECT(revoked, "Revocation should succeed");

  // Validate should fail after revocation
  auto result = manager.validate(pair.raw_key);
  KJ_EXPECT(result == kj::none, "Revoked key should not validate");

  // Revoking again should fail
  bool revoked_again = manager.revoke(pair.key_id);
  KJ_EXPECT(!revoked_again, "Re-revocation should fail");
}

KJ_TEST("ApiKeyManager: List keys by user") {
  ApiKeyManager manager;

  // Create keys for user1
  kj::Vector<kj::String> perms1;
  perms1.add(kj::str("read"));
  auto pair1 = manager.create_key("user1"_kj, "key1"_kj, kj::mv(perms1));

  kj::Vector<kj::String> perms2;
  perms2.add(kj::str("write"));
  auto pair2 = manager.create_key("user1"_kj, "key2"_kj, kj::mv(perms2));

  // Create key for user2
  kj::Vector<kj::String> perms3;
  auto pair3 = manager.create_key("user2"_kj, "key3"_kj, kj::mv(perms3));

  // List keys for user1
  auto user1_keys = manager.list_keys("user1"_kj);
  KJ_EXPECT(user1_keys.size() == 2, "user1 should have 2 keys");

  // List keys for user2
  auto user2_keys = manager.list_keys("user2"_kj);
  KJ_EXPECT(user2_keys.size() == 1, "user2 should have 1 key");

  // List keys for non-existent user
  auto user3_keys = manager.list_keys("user3"_kj);
  KJ_EXPECT(user3_keys.size() == 0, "user3 should have 0 keys");
}

KJ_TEST("ApiKeyManager: Permission check") {
  ApiKeyManager manager;

  kj::Vector<kj::String> permissions;
  permissions.add(kj::str("read"));
  permissions.add(kj::str("trade"));

  auto pair = manager.create_key("user123"_kj, "test-key"_kj, kj::mv(permissions));

  KJ_IF_SOME(validated, manager.validate(pair.raw_key)) {
    KJ_EXPECT(ApiKeyManager::has_permission(*validated, "read"_kj));
    KJ_EXPECT(ApiKeyManager::has_permission(*validated, "trade"_kj));
    KJ_EXPECT(!ApiKeyManager::has_permission(*validated, "admin"_kj));
    KJ_EXPECT(!ApiKeyManager::has_permission(*validated, "withdraw"_kj));
  }
  else {
    KJ_FAIL_EXPECT("Key validation failed");
  }
}

KJ_TEST("ApiKeyManager: Active key count") {
  ApiKeyManager manager;

  KJ_EXPECT(manager.active_key_count() == 0);

  kj::Vector<kj::String> perms1;
  auto pair1 = manager.create_key("user1"_kj, "key1"_kj, kj::mv(perms1));
  KJ_EXPECT(manager.active_key_count() == 1);

  kj::Vector<kj::String> perms2;
  auto pair2 = manager.create_key("user1"_kj, "key2"_kj, kj::mv(perms2));
  KJ_EXPECT(manager.active_key_count() == 2);

  // Revoke one key
  manager.revoke(pair1.key_id);
  KJ_EXPECT(manager.active_key_count() == 1);

  // Revoke the other key
  manager.revoke(pair2.key_id);
  KJ_EXPECT(manager.active_key_count() == 0);
}

KJ_TEST("ApiKeyManager: Unique key IDs") {
  ApiKeyManager manager;

  // Create many keys and verify uniqueness
  kj::HashMap<kj::String, bool> key_ids;

  for (int i = 0; i < 100; ++i) {
    kj::Vector<kj::String> perms;
    auto pair = manager.create_key("user"_kj, kj::str("key-", i), kj::mv(perms));

    KJ_IF_SOME(existing, key_ids.find(pair.key_id)) {
      (void)existing;
      KJ_FAIL_EXPECT("Duplicate key ID generated", pair.key_id.cStr());
    }

    key_ids.upsert(kj::str(pair.key_id), true, [](bool&, bool&&) {});
  }

  KJ_EXPECT(key_ids.size() == 100);
}

KJ_TEST("ApiKeyManager: Last used timestamp updates") {
  ApiKeyManager manager;

  kj::Vector<kj::String> perms;
  auto pair = manager.create_key("user123"_kj, "test-key"_kj, kj::mv(perms));

  auto first_validation = manager.validate(pair.raw_key);
  KJ_IF_SOME(first, first_validation) {
    auto first_time = first->last_used;

    // Small delay
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto second_validation = manager.validate(pair.raw_key);
    KJ_IF_SOME(second, second_validation) {
      auto second_time = second->last_used;

      // Timestamps should be different (or at least not earlier)
      KJ_EXPECT(second_time >= first_time);
    }
    else {
      KJ_FAIL_EXPECT("Second validation failed");
    }
  }
  else {
    KJ_FAIL_EXPECT("First validation failed");
  }
}

KJ_TEST("ApiKeyManager: Multiple keys for same user") {
  ApiKeyManager manager;

  kj::Vector<kj::String> perms1;
  perms1.add(kj::str("read"));
  auto pair1 = manager.create_key("user123"_kj, "key1"_kj, kj::mv(perms1));

  kj::Vector<kj::String> perms2;
  perms2.add(kj::str("write"));
  auto pair2 = manager.create_key("user123"_kj, "key2"_kj, kj::mv(perms2));

  // Both keys should be different
  KJ_EXPECT(pair1.key_id != pair2.key_id);
  KJ_EXPECT(pair1.raw_key != pair2.raw_key);

  // Both should validate
  KJ_IF_SOME(v1, manager.validate(pair1.raw_key)) {
    KJ_EXPECT(ApiKeyManager::has_permission(*v1, "read"_kj));
    KJ_EXPECT(!ApiKeyManager::has_permission(*v1, "write"_kj));
  }
  else {
    KJ_FAIL_EXPECT("Key 1 validation failed");
  }

  KJ_IF_SOME(v2, manager.validate(pair2.raw_key)) {
    KJ_EXPECT(ApiKeyManager::has_permission(*v2, "write"_kj));
    KJ_EXPECT(!ApiKeyManager::has_permission(*v2, "read"_kj));
  }
  else {
    KJ_FAIL_EXPECT("Key 2 validation failed");
  }
}

KJ_TEST("ApiKeyManager: Thread safety - concurrent key creation") {
  ApiKeyManager manager;

  // Test concurrent key creation
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};

  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&manager, &success_count, i]() {
      for (int j = 0; j < 10; ++j) {
        kj::Vector<kj::String> perms;
        perms.add(kj::str("perm"));
        auto pair = manager.create_key(kj::str("user-", i), kj::str("key-", j), kj::mv(perms));
        if (pair.key_id.size() > 0 && pair.raw_key.size() > 0) {
          success_count++;
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  KJ_EXPECT(success_count == 100, "All key creations should succeed");
  KJ_EXPECT(manager.active_key_count() == 100);
}

KJ_TEST("ApiKeyManager: Thread safety - concurrent validation") {
  ApiKeyManager manager;

  // Create some keys
  kj::Vector<kj::String> pairs;
  for (int i = 0; i < 10; ++i) {
    kj::Vector<kj::String> perms;
    auto pair = manager.create_key("user"_kj, kj::str("key-", i), kj::mv(perms));
    pairs.add(kj::mv(pair.raw_key));
  }

  // Concurrent validation
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};

  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&manager, &pairs, &success_count, i]() {
      for (int j = 0; j < 100; ++j) {
        auto result = manager.validate(pairs[i]);
        if (result != kj::none) {
          success_count++;
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  KJ_EXPECT(success_count == 1000, "All validations should succeed");
}

KJ_TEST("ApiKeyManager: Key format validation") {
  ApiKeyManager manager;

  // Create a key
  kj::Vector<kj::String> perms;
  auto pair = manager.create_key("user"_kj, "key"_kj, kj::mv(perms));

  // Valid key should work
  KJ_IF_SOME(validated, manager.validate(pair.raw_key)) {
    (void)validated;
  }
  else {
    KJ_FAIL_EXPECT("Valid key should validate");
  }

  // Invalid length should return none
  auto result1 = manager.validate("short"_kj);
  KJ_EXPECT(result1 == kj::none, "Invalid length should return none");

  // Invalid hex should return none
  auto result2 =
      manager.validate("gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg"_kj);
  KJ_EXPECT(result2 == kj::none, "Invalid hex should return none");
}

KJ_TEST("ApiKeyManager: Empty permissions") {
  ApiKeyManager manager;

  kj::Vector<kj::String> perms; // Empty permissions
  auto pair = manager.create_key("user"_kj, "key"_kj, kj::mv(perms));

  KJ_IF_SOME(validated, manager.validate(pair.raw_key)) {
    KJ_EXPECT(validated->permissions.size() == 0);
    KJ_EXPECT(!ApiKeyManager::has_permission(*validated, "any"_kj));
  }
  else {
    KJ_FAIL_EXPECT("Key validation failed");
  }
}

} // anonymous namespace
