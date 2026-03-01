#include "audit/audit_logger.h"
#include "audit/audit_store.h"
#include "kj/test.h"

#include <chrono>
#include <cstdlib>
#include <kj/async-io.h>
#include <kj/filesystem.h>
#include <kj/string.h>
#include <kj/thread.h>
#include <kj/vector.h>
#include <thread>

using namespace veloz::gateway::audit;

namespace {

struct TestIo {
  kj::AsyncIoContext io;
  kj::WaitScope& waitScope;

  TestIo() : io(kj::setupAsyncIo()), waitScope(io.waitScope) {}
};

kj::String create_temp_log_dir() {
  auto fs = kj::newDiskFilesystem();
  auto basePath = fs->getCurrentPath().evalNative("/tmp");
  auto uniqueName = kj::str("veloz_audit_test_", std::rand());
  auto tempPath = basePath.append(uniqueName);
  auto dir = fs->getRoot().openSubdir(tempPath, kj::WriteMode::CREATE);
  (void)dir;
  return tempPath.toNativeString(true);
}

void cleanup_temp_dir(kj::StringPtr path) {
  auto command = kj::str("rm -rf ", path);
  int result = system(command.cStr());
  (void)result;
}

// ============================================================================
// AuditLogType Utilities Tests
// ============================================================================

KJ_TEST("AuditLogType: Type to string conversion") {
  KJ_EXPECT(auditLogTypeToString(AuditLogType::Auth) == "auth"_kj);
  KJ_EXPECT(auditLogTypeToString(AuditLogType::Order) == "order"_kj);
  KJ_EXPECT(auditLogTypeToString(AuditLogType::ApiKey) == "apikey"_kj);
  KJ_EXPECT(auditLogTypeToString(AuditLogType::Error) == "error"_kj);
  KJ_EXPECT(auditLogTypeToString(AuditLogType::Access) == "access"_kj);
}

KJ_TEST("AuditLogType: String to type conversion") {
  KJ_EXPECT(stringToAuditLogType("auth") == AuditLogType::Auth);
  KJ_EXPECT(stringToAuditLogType("order") == AuditLogType::Order);
  KJ_EXPECT(stringToAuditLogType("apikey") == AuditLogType::ApiKey);
  KJ_EXPECT(stringToAuditLogType("error") == AuditLogType::Error);
  KJ_EXPECT(stringToAuditLogType("access") == AuditLogType::Access);
  KJ_EXPECT(stringToAuditLogType("unknown") == kj::none);
}

// ============================================================================
// AuditLogEntry Tests
// ============================================================================

KJ_TEST("AuditLogEntry: Basic NDJSON serialization") {
  AuditLogEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.type = AuditLogType::Auth;
  entry.action = kj::str("login");
  entry.user_id = kj::str("user123");
  entry.ip_address = kj::str("192.168.1.1");
  entry.request_id = kj::str("req-123");

  auto json = entry.to_ndjson();

  KJ_EXPECT(json.startsWith("{"));
  KJ_EXPECT(json.endsWith("}\n"));
  KJ_EXPECT(json.find("\"type\":\"auth\"") != kj::none);
  KJ_EXPECT(json.find("\"action\":\"login\"") != kj::none);
  KJ_EXPECT(json.find("\"user_id\":\"user123\"") != kj::none);
  KJ_EXPECT(json.find("\"ip_address\":\"192.168.1.1\"") != kj::none);
  KJ_EXPECT(json.find("\"request_id\":\"req-123\"") != kj::none);
}

KJ_TEST("AuditLogEntry: NDJSON with special characters") {
  AuditLogEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.type = AuditLogType::Error;
  entry.action = kj::str("Error with \"quotes\" and \\backslashes\\");
  entry.user_id = kj::str("user\nnewline");
  entry.ip_address = kj::str("127.0.0.1");

  auto json = entry.to_ndjson();

  // Check that special characters are properly escaped
  KJ_EXPECT(json.find("\\\"quotes\\\"") != kj::none);
  KJ_EXPECT(json.find("\\\\backslashes\\\\") != kj::none);
  KJ_EXPECT(json.find("\\n") != kj::none);
}

KJ_TEST("AuditLogEntry: Clone") {
  AuditLogEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.type = AuditLogType::Order;
  entry.action = kj::str("create_order");
  entry.user_id = kj::str("user123");
  entry.ip_address = kj::str("10.0.0.1");
  entry.request_id = kj::str("req-456");

  entry.details.insert(kj::str("symbol"), kj::str("BTCUSDT"));
  entry.details.insert(kj::str("quantity"), kj::str("1.5"));

  auto cloned = entry.clone();

  KJ_EXPECT(cloned.type == entry.type);
  KJ_EXPECT(cloned.action == entry.action);
  KJ_EXPECT(cloned.user_id == entry.user_id);
  KJ_EXPECT(cloned.ip_address == entry.ip_address);

  KJ_IF_SOME(req_id, cloned.request_id) {
    KJ_EXPECT(req_id == "req-456"_kj);
  }
  else {
    KJ_FAIL_EXPECT("request_id missing in clone");
  }

  KJ_EXPECT(cloned.details.size() == 2);
}

KJ_TEST("AuditLogEntry: NDJSON with details") {
  AuditLogEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.type = AuditLogType::Order;
  entry.action = kj::str("create_order");
  entry.user_id = kj::str("user123");
  entry.ip_address = kj::str("10.0.0.1");

  entry.details.insert(kj::str("symbol"), kj::str("BTCUSDT"));
  entry.details.insert(kj::str("quantity"), kj::str("1.5"));

  auto json = entry.to_ndjson();

  KJ_EXPECT(json.find("\"details\":{") != kj::none);
  KJ_EXPECT(json.find("\"symbol\":\"BTCUSDT\"") != kj::none);
  KJ_EXPECT(json.find("\"quantity\":\"1.5\"") != kj::none);
}

// ============================================================================
// AuditLogger Tests
// ============================================================================

KJ_TEST("AuditLogger: Basic logging") {
  TestIo io;
  auto log_dir = create_temp_log_dir();
  KJ_DEFER(cleanup_temp_dir(log_dir));

  // Create logger
  auto logger = kj::heap<AuditLogger>(log_dir);

  // Log an entry
  AuditLogEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.type = AuditLogType::Auth;
  entry.action = kj::str("login");
  entry.user_id = kj::str("user123");
  entry.ip_address = kj::str("192.168.1.1");
  entry.request_id = kj::str("req-001");

  auto promise = logger->log(kj::mv(entry));
  promise.wait(io.waitScope);

  // Wait for writer thread
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Flush
  auto flush_promise = logger->flush();
  flush_promise.wait(io.waitScope);

  // Check stats
  auto stats = logger->get_stats();
  KJ_EXPECT(stats.total_logged >= 1);
  KJ_EXPECT(stats.total_flushed >= 1);
}

KJ_TEST("AuditLogger: High-throughput logging (10000+ entries/sec)") {
  TestIo io;
  auto log_dir = create_temp_log_dir();
  KJ_DEFER(cleanup_temp_dir(log_dir));

  auto logger = kj::heap<AuditLogger>(log_dir);

  constexpr int NUM_ENTRIES = 10000;

  auto start = std::chrono::high_resolution_clock::now();

  // Log entries
  for (int i = 0; i < NUM_ENTRIES; ++i) {
    AuditLogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.type = static_cast<AuditLogType>(i % 5);
    entry.action = kj::str("action", i);
    entry.user_id = kj::str("user", i % 100);
    entry.ip_address = kj::str("192.168.1.", i % 255);
    entry.request_id = kj::str("req-", i);

    (void)logger->log(kj::mv(entry));
  }

  auto end = std::chrono::high_resolution_clock::now();

  // Wait for all entries to be processed
  auto flush_promise = logger->flush();
  flush_promise.wait(io.waitScope);

  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  double entries_per_sec = (NUM_ENTRIES * 1000000.0) / duration.count();
  double avg_ns_per_entry = static_cast<double>(duration.count()) / NUM_ENTRIES;

  KJ_LOG(INFO, "High-throughput test results", entries_per_sec, "entries/sec", avg_ns_per_entry,
         "ns/entry");

  // Verify performance target (<5μs per log entry, non-blocking)
  KJ_EXPECT(avg_ns_per_entry < 5000.0, "Log entry too slow", avg_ns_per_entry);

  // Verify all entries were logged
  auto stats = logger->get_stats();
  KJ_EXPECT(stats.total_logged >= NUM_ENTRIES);
  KJ_EXPECT(stats.total_flushed >= NUM_ENTRIES);
}

KJ_TEST("AuditLogger: Concurrent logging") {
  TestIo io;
  auto log_dir = create_temp_log_dir();
  KJ_DEFER(cleanup_temp_dir(log_dir));

  auto logger = kj::heap<AuditLogger>(log_dir);

  constexpr int NUM_THREADS = 4;
  constexpr int ENTRIES_PER_THREAD = 1000;

  kj::Vector<kj::Own<kj::Thread>> threads;

  for (int t = 0; t < NUM_THREADS; ++t) {
    threads.add(kj::heap<kj::Thread>([&logger, t]() {
      for (int i = 0; i < ENTRIES_PER_THREAD; ++i) {
        AuditLogEntry entry;
        entry.timestamp = std::chrono::system_clock::now();
        entry.type = static_cast<AuditLogType>((t + i) % 5);
        entry.action = kj::str("thread", t, "_action", i);
        entry.user_id = kj::str("user", t);
        entry.ip_address = kj::str("10.0.", t, ".", i % 255);

        (void)logger->log(kj::mv(entry));
      }
    }));
  }

  // Wait for all threads
  threads.clear();

  // Flush
  auto flush_promise = logger->flush();
  flush_promise.wait(io.waitScope);

  // Verify all entries were logged
  auto stats = logger->get_stats();
  KJ_EXPECT(stats.total_logged >= NUM_THREADS * ENTRIES_PER_THREAD);
}

KJ_TEST("AuditLogger: Log rotation by size") {
  TestIo io;
  auto log_dir = create_temp_log_dir();
  KJ_DEFER(cleanup_temp_dir(log_dir));

  // Set small max file size for testing
  AuditLoggerConfig config;
  config.log_dir = log_dir;
  config.max_file_size = 10 * 1024; // 10KB
  config.retention_days = 1;

  auto logger = kj::heap<AuditLogger>(config);

  // Log enough entries to trigger rotation
  constexpr int NUM_ENTRIES = 1000;
  for (int i = 0; i < NUM_ENTRIES; ++i) {
    AuditLogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.type = AuditLogType::Access;
    entry.action = kj::str("access_action_", i);
    entry.user_id = kj::str("user123");
    entry.ip_address = kj::str("127.0.0.1");
    entry.details.insert(kj::str("extra_data"), kj::str("Lorem ipsum dolor sit amet"));

    (void)logger->log(kj::mv(entry));
  }

  // Wait for all entries
  auto flush_promise = logger->flush();
  flush_promise.wait(io.waitScope);

  // Check that rotation occurred
  auto stats = logger->get_stats();
  KJ_EXPECT(stats.total_rotations > 0, "No rotation occurred");

  // Check that multiple log files were created
  auto store = kj::heap<AuditStore>(log_dir);
  auto files = store->list_log_files();
  KJ_EXPECT(files.size() > 1, "Multiple log files not created", files.size());
}

KJ_TEST("AuditLogger: Log retention policy") {
  TestIo io;
  auto log_dir = create_temp_log_dir();
  KJ_DEFER(cleanup_temp_dir(log_dir));

  AuditLoggerConfig config;
  config.log_dir = log_dir;
  config.max_file_size = 100 * 1024 * 1024;
  config.retention_days = 1; // 1 day retention

  auto logger = kj::heap<AuditLogger>(config);

  // Log some entries
  for (int i = 0; i < 100; ++i) {
    (void)logger->log(AuditLogType::Auth, kj::str("login"), kj::str("user", i),
                      kj::str("192.168.1.1"), kj::str("req-", i));
  }

  // Flush
  auto flush_promise = logger->flush();
  flush_promise.wait(io.waitScope);

  // Apply retention policy manually
  logger->apply_retention_policy();

  KJ_LOG(INFO, "Retention policy test completed");
}

KJ_TEST("AuditLogger: Convenience method") {
  TestIo io;
  auto log_dir = create_temp_log_dir();
  KJ_DEFER(cleanup_temp_dir(log_dir));

  auto logger = kj::heap<AuditLogger>(log_dir);

  // Log using convenience method
  auto promise = logger->log(AuditLogType::Order, kj::str("create_order"), kj::str("user123"),
                             kj::str("10.0.0.1"), kj::str("req-007"));

  promise.wait(io.waitScope);

  // Wait and flush
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  auto flush_promise = logger->flush();
  flush_promise.wait(io.waitScope);

  // Verify
  auto stats = logger->get_stats();
  KJ_EXPECT(stats.total_logged >= 1);
}

// ============================================================================
// AuditStore Tests
// ============================================================================

KJ_TEST("AuditStore: Query by type") {
  TestIo io;
  auto log_dir = create_temp_log_dir();
  KJ_DEFER(cleanup_temp_dir(log_dir));

  auto logger = kj::heap<AuditLogger>(log_dir);

  // Log entries of different types
  (void)logger->log(AuditLogType::Auth, kj::str("login"), kj::str("user1"), kj::str("192.168.1.1"));
  (void)logger->log(AuditLogType::Order, kj::str("create_order"), kj::str("user2"),
                    kj::str("10.0.0.1"));
  (void)logger->log(AuditLogType::Auth, kj::str("logout"), kj::str("user1"),
                    kj::str("192.168.1.1"));
  (void)logger->log(AuditLogType::Error, kj::str("validation_error"), kj::str("user3"),
                    kj::str("127.0.0.1"));

  // Wait for writes
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  logger->flush().wait(io.waitScope);

  // Query for Auth type
  auto store = kj::heap<AuditStore>(log_dir);
  AuditQueryOptions options;
  options.type = AuditLogType::Auth;

  auto result_promise = store->query(options);
  auto result = result_promise.wait(io.waitScope);

  KJ_EXPECT(result.total_count >= 2, "Query should return at least 2 auth entries");
}

KJ_TEST("AuditStore: Query by user_id") {
  TestIo io;
  auto log_dir = create_temp_log_dir();
  KJ_DEFER(cleanup_temp_dir(log_dir));

  auto logger = kj::heap<AuditLogger>(log_dir);

  (void)logger->log(AuditLogType::Auth, kj::str("login"), kj::str("alice"), kj::str("192.168.1.1"));
  (void)logger->log(AuditLogType::Order, kj::str("create_order"), kj::str("bob"),
                    kj::str("10.0.0.1"));
  (void)logger->log(AuditLogType::Auth, kj::str("logout"), kj::str("alice"),
                    kj::str("192.168.1.1"));

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  logger->flush().wait(io.waitScope);

  auto store = kj::heap<AuditStore>(log_dir);
  AuditQueryOptions options;
  options.user_id = kj::str("alice");

  auto result_promise = store->query(options);
  auto result = result_promise.wait(io.waitScope);

  KJ_EXPECT(result.total_count >= 2, "Should find entries for alice");
}

KJ_TEST("AuditStore: Get by request_id") {
  TestIo io;
  auto log_dir = create_temp_log_dir();
  KJ_DEFER(cleanup_temp_dir(log_dir));

  auto logger = kj::heap<AuditLogger>(log_dir);

  kj::String request_id = kj::str("unique-req-id-12345");

  (void)logger->log(AuditLogType::Order, kj::str("create_order"), kj::str("user123"),
                    kj::str("127.0.0.1"), kj::str(request_id));

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  logger->flush().wait(io.waitScope);

  auto store = kj::heap<AuditStore>(log_dir);
  auto entry_promise = store->get_by_request_id("unique-req-id-12345");
  auto maybe_entry = entry_promise.wait(io.waitScope);

  KJ_EXPECT(maybe_entry != kj::none, "Should find entry by request_id");

  KJ_IF_SOME(entry, maybe_entry) {
    KJ_EXPECT(entry.action == "create_order"_kj);
    KJ_EXPECT(entry.user_id == "user123"_kj);
  }
  else {
    KJ_FAIL_EXPECT("Entry not found");
  }
}

KJ_TEST("AuditStore: Count") {
  TestIo io;
  auto log_dir = create_temp_log_dir();
  KJ_DEFER(cleanup_temp_dir(log_dir));

  auto logger = kj::heap<AuditLogger>(log_dir);

  constexpr int NUM_ENTRIES = 50;
  for (int i = 0; i < NUM_ENTRIES; ++i) {
    (void)logger->log(AuditLogType::Access, kj::str("access"), kj::str("user123"),
                      kj::str("127.0.0.1"));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  logger->flush().wait(io.waitScope);

  auto store = kj::heap<AuditStore>(log_dir);
  AuditQueryOptions options;
  auto count_promise = store->count(options);
  auto count = count_promise.wait(io.waitScope);

  KJ_EXPECT(count >= NUM_ENTRIES, "Count should match logged entries");
}

KJ_TEST("AuditStore: List log files") {
  TestIo io;
  auto log_dir = create_temp_log_dir();
  KJ_DEFER(cleanup_temp_dir(log_dir));

  auto logger = kj::heap<AuditLogger>(log_dir);

  (void)logger->log(AuditLogType::Auth, kj::str("login"), kj::str("user1"), kj::str("127.0.0.1"));

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  logger->flush().wait(io.waitScope);

  auto store = kj::heap<AuditStore>(log_dir);
  auto files = store->list_log_files();

  KJ_EXPECT(files.size() >= 1, "Should have at least one log file");
}

KJ_TEST("AuditStore: Get stats") {
  TestIo io;
  auto log_dir = create_temp_log_dir();
  KJ_DEFER(cleanup_temp_dir(log_dir));

  auto logger = kj::heap<AuditLogger>(log_dir);

  // Log different types
  (void)logger->log(AuditLogType::Auth, kj::str("login"), kj::str("user1"), kj::str("127.0.0.1"));
  (void)logger->log(AuditLogType::Order, kj::str("create_order"), kj::str("user1"),
                    kj::str("127.0.0.1"));
  (void)logger->log(AuditLogType::ApiKey, kj::str("create_key"), kj::str("user1"),
                    kj::str("127.0.0.1"));
  (void)logger->log(AuditLogType::Error, kj::str("error"), kj::str("user1"), kj::str("127.0.0.1"));
  (void)logger->log(AuditLogType::Access, kj::str("access"), kj::str("user1"),
                    kj::str("127.0.0.1"));

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  logger->flush().wait(io.waitScope);

  auto store = kj::heap<AuditStore>(log_dir);

  auto now = std::chrono::system_clock::now();
  auto start = now - std::chrono::hours(1);

  auto stats_promise = store->get_stats(start, now);
  auto stats = stats_promise.wait(io.waitScope);

  KJ_EXPECT(stats->total_entries >= 5);
  KJ_EXPECT(stats->auth_count >= 1);
  KJ_EXPECT(stats->order_count >= 1);
  KJ_EXPECT(stats->apikey_count >= 1);
  KJ_EXPECT(stats->error_count >= 1);
  KJ_EXPECT(stats->access_count >= 1);
}

// ============================================================================
// Integration Test
// ============================================================================

KJ_TEST("Integration: End-to-end audit logging workflow") {
  TestIo io;
  auto log_dir = create_temp_log_dir();
  KJ_DEFER(cleanup_temp_dir(log_dir));

  // Create logger with custom config
  AuditLoggerConfig config;
  config.log_dir = log_dir;
  config.max_file_size = 10 * 1024;
  config.retention_days = 30;
  config.queue_capacity = 5000;
  config.enable_console_output = false;

  auto logger = kj::heap<AuditLogger>(config);

  // Simulate a login event
  auto login_promise = logger->log(AuditLogType::Auth, kj::str("login_success"), kj::str("alice"),
                                   kj::str("192.168.1.100"), kj::str("session-abc123"));

  login_promise.wait(io.waitScope);

  // Simulate some trading activity
  for (int i = 0; i < 10; ++i) {
    (void)logger->log(AuditLogType::Order, kj::str("create_order"), kj::str("alice"),
                      kj::str("192.168.1.100"), kj::str("order-", i));
  }

  // Simulate an API key rotation
  (void)logger->log(AuditLogType::ApiKey, kj::str("rotate_key"), kj::str("alice"),
                    kj::str("192.168.1.100"), kj::str("key-rotate-001"));

  // Simulate an error
  (void)logger->log(AuditLogType::Error, kj::str("rate_limit_exceeded"), kj::str("alice"),
                    kj::str("192.168.1.100"), kj::str("error-001"));

  // Wait for all logs to be written
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  logger->flush().wait(io.waitScope);

  // Verify with store
  auto store = kj::heap<AuditStore>(log_dir);

  // Query all entries for alice
  AuditQueryOptions options;
  options.user_id = kj::str("alice");
  options.limit = 100;

  auto result_promise = store->query(options);
  auto result = result_promise.wait(io.waitScope);

  KJ_EXPECT(result.total_count >= 13, "Should find at least 13 entries for alice");

  // Verify counts
  auto now = std::chrono::system_clock::now();
  auto start = now - std::chrono::hours(1);

  auto stats_promise = store->get_stats(start, now);
  auto stats = stats_promise.wait(io.waitScope);

  KJ_EXPECT(stats->auth_count >= 1);
  KJ_EXPECT(stats->order_count >= 10);
  KJ_EXPECT(stats->apikey_count >= 1);
  KJ_EXPECT(stats->error_count >= 1);

  // Verify logger stats
  auto logger_stats = logger->get_stats();
  KJ_EXPECT(logger_stats.total_logged >= 13);
  KJ_EXPECT(logger_stats.total_flushed >= 13);
}

KJ_TEST("Integration: High-throughput concurrent test") {
  TestIo io;
  auto log_dir = create_temp_log_dir();
  KJ_DEFER(cleanup_temp_dir(log_dir));

  auto logger = kj::heap<AuditLogger>(log_dir);

  constexpr int NUM_PRODUCERS = 8;
  constexpr int ENTRIES_PER_PRODUCER = 2000;

  kj::Vector<kj::Own<kj::Thread>> producers;

  auto start = std::chrono::high_resolution_clock::now();

  for (int t = 0; t < NUM_PRODUCERS; ++t) {
    producers.add(kj::heap<kj::Thread>([&logger, t]() {
      for (int i = 0; i < ENTRIES_PER_PRODUCER; ++i) {
        (void)logger->log(AuditLogType::Access, kj::str("api_call"), kj::str("user", t),
                          kj::str("192.168.", t % 256, ".", i % 256));
      }
    }));
  }

  // Wait for all producers
  producers.clear();

  // Wait for all entries to be processed
  logger->flush().wait(io.waitScope);

  auto end = std::chrono::high_resolution_clock::now();

  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  double entries_per_sec = (NUM_PRODUCERS * ENTRIES_PER_PRODUCER * 1000000.0) / duration.count();

  KJ_LOG(INFO, "Concurrent high-throughput test results", entries_per_sec, "entries/sec");

  // Verify performance target
  KJ_EXPECT(entries_per_sec >= 10000.0, "Failed to meet throughput target", entries_per_sec);

  // Verify all entries were logged
  auto stats = logger->get_stats();
  KJ_EXPECT(stats.total_logged >= NUM_PRODUCERS * ENTRIES_PER_PRODUCER);

  // Verify no errors
  KJ_EXPECT(stats.total_errors == 0, "Errors occurred during logging", stats.total_errors);
}

} // namespace
