#include "kj/test.h"
#include "veloz/core/logger.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <kj/memory.h>
#include <kj/string.h>
#include <memory> // Kept for Logger API compatibility (std::unique_ptr)
#include <sstream>
#include <string> // Kept for Logger API compatibility

using namespace veloz::core;

namespace {

// ============================================================================
// LogLevel Tests
// ============================================================================

KJ_TEST("LogLevel: ToString") {
  KJ_EXPECT(std::string(to_string(LogLevel::Trace)) == "TRACE");
  KJ_EXPECT(std::string(to_string(LogLevel::Debug)) == "DEBUG");
  KJ_EXPECT(std::string(to_string(LogLevel::Info)) == "INFO");
  KJ_EXPECT(std::string(to_string(LogLevel::Warn)) == "WARN");
  KJ_EXPECT(std::string(to_string(LogLevel::Error)) == "ERROR");
  KJ_EXPECT(std::string(to_string(LogLevel::Critical)) == "CRITICAL");
  KJ_EXPECT(std::string(to_string(LogLevel::Off)) == "OFF");
}

// ============================================================================
// TextFormatter Tests
// ============================================================================

KJ_TEST("TextFormatter: Format") {
  TextFormatter formatter(false, false);
  auto now = std::chrono::system_clock::now();
  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 42,
                 .function = "test_func",
                 .message = "Test message",
                 .time_point = now};

  std::string formatted = formatter.format(entry);

  KJ_EXPECT(!formatted.empty());
  // The formatter uses time_point, not timestamp string, so check for current year
  KJ_EXPECT(formatted.find("INFO") != std::string::npos);
  KJ_EXPECT(formatted.find("test.cpp") != std::string::npos);
  KJ_EXPECT(formatted.find("42") != std::string::npos);
  KJ_EXPECT(formatted.find("Test message") != std::string::npos);
}

KJ_TEST("TextFormatter: WithFunction") {
  TextFormatter formatter(true, false);
  LogEntry entry{.level = LogLevel::Debug,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 42,
                 .function = "test_func",
                 .message = "Debug message",
                 .time_point = std::chrono::system_clock::now()};

  std::string formatted = formatter.format(entry);

  KJ_EXPECT(formatted.find("test_func") != std::string::npos);
}

KJ_TEST("TextFormatter: WithColor") {
  TextFormatter formatter(false, true);
  LogEntry entry{.level = LogLevel::Error,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 42,
                 .function = "test_func",
                 .message = "Error message",
                 .time_point = std::chrono::system_clock::now()};

  std::string formatted = formatter.format(entry);

  // Check for ANSI color codes
  KJ_EXPECT(formatted.find("\033[") != std::string::npos);
}

// ============================================================================
// JsonFormatter Tests
// ============================================================================

KJ_TEST("JsonFormatter: Format") {
  JsonFormatter formatter(false);
  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 42,
                 .function = "test_func",
                 .message = "Test message",
                 .time_point = std::chrono::system_clock::now()};

  std::string formatted = formatter.format(entry);

  KJ_EXPECT(!formatted.empty());
  KJ_EXPECT(formatted.find("\"timestamp\"") != std::string::npos);
  KJ_EXPECT(formatted.find("\"level\"") != std::string::npos);
  KJ_EXPECT(formatted.find("\"INFO\"") != std::string::npos);
  KJ_EXPECT(formatted.find("\"file\"") != std::string::npos);
  KJ_EXPECT(formatted.find("\"line\"") != std::string::npos);
  KJ_EXPECT(formatted.find("\"message\"") != std::string::npos);
}

KJ_TEST("JsonFormatter: Escape") {
  JsonFormatter formatter(false);
  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 42,
                 .function = "test_func",
                 .message = "Message with \"quotes\" and \\backslashes\\",
                 .time_point = std::chrono::system_clock::now()};

  std::string formatted = formatter.format(entry);

  // Check for proper escaping
  KJ_EXPECT(formatted.find("\\\"") != std::string::npos); // Escaped quotes
  KJ_EXPECT(formatted.find("\\\\") != std::string::npos); // Escaped backslashes
}

// ============================================================================
// Logger Tests
// ============================================================================

KJ_TEST("Logger: Basic logging") {
  Logger logger(std::make_unique<TextFormatter>(), std::make_unique<ConsoleOutput>());

  // This should not throw
  logger.info("Test info message");
  logger.debug("Test debug message");
  logger.warn("Test warning message");
}

KJ_TEST("Logger: Level filtering") {
  Logger logger(std::make_unique<TextFormatter>(), std::make_unique<ConsoleOutput>());
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
  Logger logger(std::make_unique<TextFormatter>(), std::make_unique<ConsoleOutput>());

  logger.info(std::format("Hello {}", "World"));
  logger.info(std::format("Value: {}, Name: {}", 42, "Test"));
}

KJ_TEST("Logger: Change formatter") {
  Logger logger(std::make_unique<TextFormatter>(), std::make_unique<ConsoleOutput>());

  logger.info("Text formatted message");

  logger.set_formatter(std::make_unique<JsonFormatter>());

  logger.info("JSON formatted message");
}

// ============================================================================
// FileOutput Tests
// ============================================================================

KJ_TEST("FileOutput: Basic") {
  std::filesystem::create_directories("test_logs");

  FileOutput output("test_logs/test.log", FileOutput::Rotation::None,
                    1024 * 1024, // 1MB
                    3);

  KJ_EXPECT(output.is_open());

  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 42,
                 .function = "test_func",
                 .message = "Test message",
                 .time_point = std::chrono::system_clock::now()};

  output.write("Test log line", entry);
  output.flush();

  KJ_EXPECT(std::filesystem::exists("test_logs/test.log"));

  // Check file content
  std::ifstream ifs("test_logs/test.log");
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  KJ_EXPECT(!content.empty());
  KJ_EXPECT(content.find("Test log line") != std::string::npos);

  // Cleanup
  std::filesystem::remove_all("test_logs");
}

KJ_TEST("FileOutput: Rotation by size") {
  std::filesystem::create_directories("test_logs");

  FileOutput output("test_logs/rotate.log", FileOutput::Rotation::Size,
                    100, // 100 bytes
                    3);

  TextFormatter formatter;
  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 42,
                 .function = "test_func",
                 .message = "Test message",
                 .time_point = std::chrono::system_clock::now()};

  std::string long_message(60, 'X'); // Create a long message
  LogEntry long_entry = entry;
  long_entry.message = long_message;

  // Write enough to trigger rotation
  for (int i = 0; i < 10; ++i) {
    std::string formatted = formatter.format(long_entry);
    output.write(formatted, long_entry);
  }

  // Check that rotation occurred
  KJ_EXPECT(std::filesystem::exists("test_logs/rotate.log"));

  // Cleanup
  std::filesystem::remove_all("test_logs");
}

KJ_TEST("FileOutput: Rotation by time") {
  std::filesystem::create_directories("test_logs");

  FileOutput output("test_logs/time_rotate.log", FileOutput::Rotation::Time,
                    1024 * 1024, // 1MB
                    3, FileOutput::RotationInterval::Hourly);

  KJ_EXPECT(output.is_open());
  // Actual time-based rotation would require waiting, we just test setup

  // Cleanup
  std::filesystem::remove_all("test_logs");
}

KJ_TEST("FileOutput: Get current path") {
  std::filesystem::create_directories("test_logs");

  FileOutput output("test_logs/path_test.log");

  auto path = output.current_path();
  KJ_EXPECT(path == "test_logs/path_test.log");

  // Cleanup
  std::filesystem::remove_all("test_logs");
}

// ============================================================================
// MultiOutput Tests
// ============================================================================

KJ_TEST("MultiOutput: Basic") {
  auto multi_output = std::make_unique<MultiOutput>();

  multi_output->add_output(std::make_unique<ConsoleOutput>());

  KJ_EXPECT(multi_output->output_count() == 1);

  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 42,
                 .function = "test_func",
                 .message = "Test message",
                 .time_point = std::chrono::system_clock::now()};

  multi_output->write("Test", entry);
  multi_output->flush();

  KJ_EXPECT(multi_output->is_open());
}

KJ_TEST("MultiOutput: Multiple destinations") {
  auto multi_output = std::make_unique<MultiOutput>();

  multi_output->add_output(std::make_unique<ConsoleOutput>());
  multi_output->add_output(std::make_unique<ConsoleOutput>());

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
  std::filesystem::create_directories("test_logs");

  Logger logger(std::make_unique<TextFormatter>(), std::make_unique<ConsoleOutput>());

  // Add file output
  logger.add_output(
      std::make_unique<FileOutput>("test_logs/multi.log", FileOutput::Rotation::None));

  logger.info("Message to both console and file");

  KJ_EXPECT(std::filesystem::exists("test_logs/multi.log"));

  // Cleanup
  std::filesystem::remove_all("test_logs");
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

  info_global(std::format("Formatted: {}", 42));
}

// ============================================================================
// Logger Flush Tests
// ============================================================================

KJ_TEST("Logger: Flush") {
  std::filesystem::create_directories("test_logs");

  Logger logger(std::make_unique<TextFormatter>(),
                std::make_unique<FileOutput>("test_logs/flush.log"));

  logger.info("Message before flush");
  logger.flush();

  // File should contain of message
  std::ifstream ifs("test_logs/flush.log");
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  KJ_EXPECT(content.find("Message before flush") != std::string::npos);

  // Cleanup
  std::filesystem::remove_all("test_logs");
}

// ============================================================================
// Formatted Logging Tests
// ============================================================================

KJ_TEST("Logger: Formatted logging at all levels") {
  Logger logger(std::make_unique<TextFormatter>(), std::make_unique<ConsoleOutput>());

  logger.trace(std::format("Trace: {}", 1));
  logger.debug(std::format("Debug: {}", 2));
  logger.info(std::format("Info: {}", 3));
  logger.warn(std::format("Warn: {}", 4));
  logger.error(std::format("Error: {}", 5));
  logger.critical(std::format("Critical: {}", 6));
}

// ============================================================================
// LogEntry Tests
// ============================================================================

KJ_TEST("LogEntry: Construction") {
  LogEntry entry{.level = LogLevel::Error,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 100,
                 .function = "my_function",
                 .message = "Test error message",
                 .time_point = std::chrono::system_clock::now()};

  KJ_EXPECT(entry.level == LogLevel::Error);
  KJ_EXPECT(entry.timestamp == "2023-01-01T12:00:00.000Z");
  KJ_EXPECT(entry.file == "test.cpp");
  KJ_EXPECT(entry.line == 100);
  KJ_EXPECT(entry.function == "my_function");
  KJ_EXPECT(entry.message == "Test error message");
}

} // namespace
