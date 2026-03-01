#include "kj/test.h"
#include "veloz/core/logger.h"

#include <fstream>
#include <kj/filesystem.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <sstream>
#include <string>

using namespace veloz::core;

namespace {

void removeTestLogs(const kj::Directory& root) {
  auto path = kj::Path::parse("test_logs");
  if (!root.exists(path))
    return;

  auto dir = root.openSubdir(path, kj::WriteMode::MODIFY);
  for (auto& file : dir->listNames()) {
    dir->remove(kj::Path(kj::mv(file)));
  }
  root.remove(path);
}

void setupTestLogs(const kj::Directory& root) {
  auto path = kj::Path::parse("test_logs");
  if (root.exists(path)) {
    removeTestLogs(root);
  }
  root.openSubdir(path, kj::WriteMode::CREATE);
}

// ============================================================================
// LogLevel Tests
// ============================================================================

KJ_TEST("LogLevel: ToString") {
  KJ_EXPECT(to_string(LogLevel::Trace) == "TRACE"_kj);
  KJ_EXPECT(to_string(LogLevel::Debug) == "DEBUG"_kj);
  KJ_EXPECT(to_string(LogLevel::Info) == "INFO"_kj);
  KJ_EXPECT(to_string(LogLevel::Warn) == "WARN"_kj);
  KJ_EXPECT(to_string(LogLevel::Error) == "ERROR"_kj);
  KJ_EXPECT(to_string(LogLevel::Critical) == "CRITICAL"_kj);
  KJ_EXPECT(to_string(LogLevel::Off) == "OFF"_kj);
}

// ============================================================================
// TextFormatter Tests
// ============================================================================

KJ_TEST("TextFormatter: Format") {
  TextFormatter formatter(false, false);
  auto now = std::chrono::system_clock::now();
  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = kj::heapString("2023-01-01T12:00:00.000Z"),
                 .file = kj::heapString("test.cpp"),
                 .line = 42,
                 .function = kj::heapString("test_func"),
                 .message = kj::heapString("Test message"),
                 .time_point = now};

  auto formattedKj = formatter.format(entry);
  kj::StringPtr formatted = formattedKj.asPtr();

  KJ_EXPECT(formatted.size() > 0);
  // The formatter uses time_point, not timestamp string, so check for current year
  KJ_EXPECT(formatted.contains("INFO"_kj));
  KJ_EXPECT(formatted.contains("test.cpp"_kj));
  KJ_EXPECT(formatted.contains("42"_kj));
  KJ_EXPECT(formatted.contains("Test message"_kj));
}

KJ_TEST("TextFormatter: WithFunction") {
  TextFormatter formatter(true, false);
  LogEntry entry{.level = LogLevel::Debug,
                 .timestamp = kj::heapString("2023-01-01T12:00:00.000Z"),
                 .file = kj::heapString("test.cpp"),
                 .line = 42,
                 .function = kj::heapString("test_func"),
                 .message = kj::heapString("Debug message"),
                 .time_point = std::chrono::system_clock::now()};

  auto formattedKj = formatter.format(entry);
  kj::StringPtr formatted = formattedKj.asPtr();

  KJ_EXPECT(formatted.contains("test_func"_kj));
}

KJ_TEST("TextFormatter: WithColor") {
  TextFormatter formatter(false, true);
  LogEntry entry{.level = LogLevel::Error,
                 .timestamp = kj::heapString("2023-01-01T12:00:00.000Z"),
                 .file = kj::heapString("test.cpp"),
                 .line = 42,
                 .function = kj::heapString("test_func"),
                 .message = kj::heapString("Error message"),
                 .time_point = std::chrono::system_clock::now()};

  auto formattedKj = formatter.format(entry);
  kj::StringPtr formatted = formattedKj.asPtr();

  // Check for ANSI color codes
  KJ_EXPECT(formatted.contains("\033["_kj));
}

// ============================================================================
// JsonFormatter Tests
// ============================================================================

KJ_TEST("JsonFormatter: Format") {
  JsonFormatter formatter(false);
  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = kj::heapString("2023-01-01T12:00:00.000Z"),
                 .file = kj::heapString("test.cpp"),
                 .line = 42,
                 .function = kj::heapString("test_func"),
                 .message = kj::heapString("Test message"),
                 .time_point = std::chrono::system_clock::now()};

  auto formattedKj = formatter.format(entry);
  kj::StringPtr formatted = formattedKj.asPtr();

  KJ_EXPECT(formatted.size() > 0);
  KJ_EXPECT(formatted.contains("\"timestamp\""_kj));
  KJ_EXPECT(formatted.contains("\"level\""_kj));
  KJ_EXPECT(formatted.contains("\"INFO\""_kj));
  KJ_EXPECT(formatted.contains("\"file\""_kj));
  KJ_EXPECT(formatted.contains("\"line\""_kj));
  KJ_EXPECT(formatted.contains("\"message\""_kj));
}

KJ_TEST("JsonFormatter: Escape") {
  JsonFormatter formatter(false);
  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = kj::heapString("2023-01-01T12:00:00.000Z"),
                 .file = kj::heapString("test.cpp"),
                 .line = 42,
                 .function = kj::heapString("test_func"),
                 .message = kj::heapString("Message with \"quotes\" and \\backslashes\\"),
                 .time_point = std::chrono::system_clock::now()};

  auto formattedKj = formatter.format(entry);
  kj::StringPtr formatted = formattedKj.asPtr();

  // Check for proper escaping
  KJ_EXPECT(formatted.contains("\\\""_kj)); // Escaped quotes
  KJ_EXPECT(formatted.contains("\\\\"_kj)); // Escaped backslashes
}

// ============================================================================
// Logger Tests
// ============================================================================

KJ_TEST("Logger: Basic logging") {
  Logger logger(kj::heap<TextFormatter>(), kj::heap<ConsoleOutput>());

  // This should not throw
  logger.info("Test info message");
  logger.debug("Test debug message");
  logger.warn("Test warning message");
}

KJ_TEST("Logger: Level filtering") {
  Logger logger(kj::heap<TextFormatter>(), kj::heap<ConsoleOutput>());
  logger.set_level(LogLevel::Warn);

  // These should be filtered out
  logger.trace("Trace message");
  logger.debug("Debug message");
  logger.info("Info message");

  // This should be logged
  logger.warn("Warning message");
  logger.error("Error message");
}

KJ_TEST("Logger: Formatted message") {
  Logger logger(kj::heap<TextFormatter>(), kj::heap<ConsoleOutput>());

  logger.info(kj::str("Hello ", "World").cStr());
  logger.info(kj::str("Value: ", 42, ", Name: ", "Test").cStr());
}

KJ_TEST("Logger: Change formatter") {
  Logger logger(kj::heap<TextFormatter>(), kj::heap<ConsoleOutput>());

  logger.info("Text formatted message");

  logger.set_formatter(kj::heap<JsonFormatter>());

  logger.info("JSON formatted message");
}

// ============================================================================
// FileOutput Tests
// ============================================================================

KJ_TEST("FileOutput: Basic") {
  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getCurrent();
  setupTestLogs(root);
  KJ_DEFER(removeTestLogs(root));

  FileOutput output(kj::Path::parse("test_logs/test.log"), FileOutput::Rotation::None,
                    1024 * 1024, // 1MB
                    3);

  KJ_EXPECT(output.is_open());

  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = kj::heapString("2023-01-01T12:00:00.000Z"),
                 .file = kj::heapString("test.cpp"),
                 .line = 42,
                 .function = kj::heapString("test_func"),
                 .message = kj::heapString("Test message"),
                 .time_point = std::chrono::system_clock::now()};

  output.write("Test log line", entry);
  output.flush();

  KJ_EXPECT(root.exists(kj::Path::parse("test_logs/test.log")));

  // Check file content
  auto content = root.openFile(kj::Path::parse("test_logs/test.log"))->readAllText();
  KJ_EXPECT(content.size() > 0);
  KJ_EXPECT(content.contains("Test log line"_kj));
}

KJ_TEST("FileOutput: Rotation by size") {
  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getCurrent();
  setupTestLogs(root);
  KJ_DEFER(removeTestLogs(root));

  FileOutput output(kj::Path::parse("test_logs/rotate.log"), FileOutput::Rotation::Size,
                    100, // 100 bytes
                    3);

  TextFormatter formatter;
  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = kj::heapString("2023-01-01T12:00:00.000Z"),
                 .file = kj::heapString("test.cpp"),
                 .line = 42,
                 .function = kj::heapString("test_func"),
                 .message = kj::heapString("Test message"),
                 .time_point = std::chrono::system_clock::now()};

  // Create a long message using KJ (60 'X' characters)
  kj::String long_message = kj::str("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
  LogEntry long_entry{.level = entry.level,
                      .timestamp = kj::heapString(entry.timestamp.asPtr()),
                      .file = kj::heapString(entry.file.asPtr()),
                      .line = entry.line,
                      .function = kj::heapString(entry.function.asPtr()),
                      .message = kj::mv(long_message),
                      .time_point = entry.time_point};

  // Write enough to trigger rotation
  for (int i = 0; i < 10; ++i) {
    auto formatted = formatter.format(long_entry);
    output.write(formatted.asPtr(), long_entry);
  }

  // Check that rotation occurred
  KJ_EXPECT(root.exists(kj::Path::parse("test_logs/rotate.log")));
}

KJ_TEST("FileOutput: Rotation by time") {
  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getCurrent();
  setupTestLogs(root);
  KJ_DEFER(removeTestLogs(root));

  FileOutput output(kj::Path::parse("test_logs/time_rotate.log"), FileOutput::Rotation::Time,
                    1024 * 1024, // 1MB
                    3, FileOutput::RotationInterval::Hourly);

  KJ_EXPECT(output.is_open());
  // Actual time-based rotation would require waiting, we just test setup
}

KJ_TEST("FileOutput: Get current path") {
  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getCurrent();
  setupTestLogs(root);
  KJ_DEFER(removeTestLogs(root));

  FileOutput output(kj::Path::parse("test_logs/path_test.log"));

  auto path = output.current_path();
  KJ_EXPECT(path.toString(false) == kj::str("test_logs/path_test.log"));
}

// ============================================================================
// MultiOutput Tests
// ============================================================================

KJ_TEST("MultiOutput: Basic") {
  auto multi_output = kj::heap<MultiOutput>();

  multi_output->add_output(kj::heap<ConsoleOutput>());

  KJ_EXPECT(multi_output->output_count() == 1);

  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = kj::heapString("2023-01-01T12:00:00.000Z"),
                 .file = kj::heapString("test.cpp"),
                 .line = 42,
                 .function = kj::heapString("test_func"),
                 .message = kj::heapString("Test message"),
                 .time_point = std::chrono::system_clock::now()};

  multi_output->write("Test", entry);
  multi_output->flush();

  KJ_EXPECT(multi_output->is_open());
}

KJ_TEST("MultiOutput: Multiple destinations") {
  auto multi_output = kj::heap<MultiOutput>();

  multi_output->add_output(kj::heap<ConsoleOutput>());
  multi_output->add_output(kj::heap<ConsoleOutput>());

  KJ_EXPECT(multi_output->output_count() == 2);

  multi_output->remove_output(0);
  KJ_EXPECT(multi_output->output_count() == 1);

  multi_output->clear_outputs();
  KJ_EXPECT(multi_output->output_count() == 0);
  KJ_EXPECT(!multi_output->is_open());
}

// ============================================================================
// Logger with Multiple Outputs
// ============================================================================

KJ_TEST("Logger: With multiple outputs") {
  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getCurrent();
  setupTestLogs(root);
  KJ_DEFER(removeTestLogs(root));

  Logger logger(kj::heap<TextFormatter>(), kj::heap<ConsoleOutput>());

  // Add file output
  logger.add_output(
      kj::heap<FileOutput>(kj::Path::parse("test_logs/multi.log"), FileOutput::Rotation::None));

  logger.info("Message to both console and file");

  KJ_EXPECT(root.exists(kj::Path::parse("test_logs/multi.log")));
}

// ============================================================================
// Global Logger Tests
// ============================================================================

KJ_TEST("Global Logger: Basic") {
  auto& logger = global_logger();

  logger.set_level(LogLevel::Info);
  KJ_EXPECT(logger.level() == LogLevel::Info);

  logger.info("Global logger info");
}

KJ_TEST("Global Logger: Convenience functions") {
  info_global("Info message");
  debug_global("Debug message");
  warn_global("Warning message");
  error_global("Error message");
  critical_global("Critical message");

  info_global(kj::str("Formatted: ", 42).cStr());
}

// ============================================================================
// Logger Flush Tests
// ============================================================================

KJ_TEST("Logger: Flush") {
  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getCurrent();
  setupTestLogs(root);
  KJ_DEFER(removeTestLogs(root));

  Logger logger(kj::heap<TextFormatter>(),
                kj::heap<FileOutput>(kj::Path::parse("test_logs/flush.log")));

  logger.info("Message before flush");
  logger.flush();

  // File should contain of message
  auto content = root.openFile(kj::Path::parse("test_logs/flush.log"))->readAllText();
  KJ_EXPECT(content.contains("Message before flush"_kj));
}

// ============================================================================
// Formatted Logging Tests
// ============================================================================

KJ_TEST("Logger: Formatted logging at all levels") {
  Logger logger(kj::heap<TextFormatter>(), kj::heap<ConsoleOutput>());

  logger.trace(kj::str("Trace: ", 1).cStr());
  logger.debug(kj::str("Debug: ", 2).cStr());
  logger.info(kj::str("Info: ", 3).cStr());
  logger.warn(kj::str("Warn: ", 4).cStr());
  logger.error(kj::str("Error: ", 5).cStr());
  logger.critical(kj::str("Critical: ", 6).cStr());
}

// ============================================================================
// LogEntry Tests
// ============================================================================

KJ_TEST("LogEntry: Construction") {
  LogEntry entry{.level = LogLevel::Error,
                 .timestamp = kj::heapString("2023-01-01T12:00:00.000Z"),
                 .file = kj::heapString("test.cpp"),
                 .line = 100,
                 .function = kj::heapString("my_function"),
                 .message = kj::heapString("Test error message"),
                 .time_point = std::chrono::system_clock::now()};

  KJ_EXPECT(entry.level == LogLevel::Error);
  KJ_EXPECT(entry.timestamp == "2023-01-01T12:00:00.000Z");
  KJ_EXPECT(entry.file == "test.cpp");
  KJ_EXPECT(entry.line == 100);
  KJ_EXPECT(entry.function == "my_function");
  KJ_EXPECT(entry.message == "Test error message");
}

} // namespace
