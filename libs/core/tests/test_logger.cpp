#include "veloz/core/logger.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>

using namespace veloz::core;

class LoggerTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create a string stream for testing
    ss_ = std::make_unique<std::stringstream>();
  }

  void TearDown() override {
    // Clean up test files
    try {
      std::filesystem::remove_all("test_logs");
    } catch (...) {
    }
  }

  std::unique_ptr<std::stringstream> ss_;
};

// ============================================================================
// LogLevel Tests
// ============================================================================

TEST(LogLevelTest, ToString) {
  EXPECT_EQ(to_string(LogLevel::Trace), "TRACE");
  EXPECT_EQ(to_string(LogLevel::Debug), "DEBUG");
  EXPECT_EQ(to_string(LogLevel::Info), "INFO");
  EXPECT_EQ(to_string(LogLevel::Warn), "WARN");
  EXPECT_EQ(to_string(LogLevel::Error), "ERROR");
  EXPECT_EQ(to_string(LogLevel::Critical), "CRITICAL");
  EXPECT_EQ(to_string(LogLevel::Off), "OFF");
}

// ============================================================================
// TextFormatter Tests
// ============================================================================

TEST_F(LoggerTest, TextFormatterFormat) {
  TextFormatter formatter(false, false);
  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 42,
                 .function = "test_func",
                 .message = "Test message",
                 .time_point = std::chrono::system_clock::now()};

  std::string formatted = formatter.format(entry);

  EXPECT_FALSE(formatted.empty());
  EXPECT_TRUE(formatted.find("2023-01-01") != std::string::npos);
  EXPECT_TRUE(formatted.find("INFO") != std::string::npos);
  EXPECT_TRUE(formatted.find("test.cpp") != std::string::npos);
  EXPECT_TRUE(formatted.find("42") != std::string::npos);
  EXPECT_TRUE(formatted.find("Test message") != std::string::npos);
}

TEST_F(LoggerTest, TextFormatterWithFunction) {
  TextFormatter formatter(true, false);
  LogEntry entry{.level = LogLevel::Debug,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 42,
                 .function = "test_func",
                 .message = "Debug message",
                 .time_point = std::chrono::system_clock::now()};

  std::string formatted = formatter.format(entry);

  EXPECT_TRUE(formatted.find("test_func") != std::string::npos);
}

TEST_F(LoggerTest, TextFormatterWithColor) {
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
  EXPECT_TRUE(formatted.find("\033[") != std::string::npos);
}

// ============================================================================
// JsonFormatter Tests
// ============================================================================

TEST_F(LoggerTest, JsonFormatterFormat) {
  JsonFormatter formatter(false);
  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 42,
                 .function = "test_func",
                 .message = "Test message",
                 .time_point = std::chrono::system_clock::now()};

  std::string formatted = formatter.format(entry);

  EXPECT_FALSE(formatted.empty());
  EXPECT_TRUE(formatted.find("\"timestamp\"") != std::string::npos);
  EXPECT_TRUE(formatted.find("\"level\"") != std::string::npos);
  EXPECT_TRUE(formatted.find("\"INFO\"") != std::string::npos);
  EXPECT_TRUE(formatted.find("\"file\"") != std::string::npos);
  EXPECT_TRUE(formatted.find("\"line\"") != std::string::npos);
  EXPECT_TRUE(formatted.find("\"message\"") != std::string::npos);
}

TEST_F(LoggerTest, JsonFormatterEscape) {
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
  EXPECT_TRUE(formatted.find("\\\"") != std::string::npos); // Escaped quotes
  EXPECT_TRUE(formatted.find("\\\\") != std::string::npos); // Escaped backslashes
}

// ============================================================================
// Logger Tests
// ============================================================================

TEST_F(LoggerTest, LoggerBasicLogging) {
  Logger logger(std::make_unique<TextFormatter>(), std::make_unique<ConsoleOutput>());

  // This should not throw
  logger.info("Test info message");
  logger.debug("Test debug message");
  logger.warn("Test warning message");
}

TEST_F(LoggerTest, LoggerLevelFiltering) {
  std::stringstream ss;
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

TEST_F(LoggerTest, LoggerFormattedMessage) {
  Logger logger(std::make_unique<TextFormatter>(), std::make_unique<ConsoleOutput>());

  logger.info(std::format("Hello {}", "World"));
  logger.info(std::format("Value: {}, Name: {}", 42, "Test"));
}

TEST_F(LoggerTest, LoggerChangeFormatter) {
  Logger logger(std::make_unique<TextFormatter>(), std::make_unique<ConsoleOutput>());

  logger.info("Text formatted message");

  logger.set_formatter(std::make_unique<JsonFormatter>());

  logger.info("JSON formatted message");
}

// ============================================================================
// FileOutput Tests
// ============================================================================

TEST_F(LoggerTest, FileOutputBasic) {
  std::filesystem::create_directories("test_logs");

  FileOutput output("test_logs/test.log", FileOutput::Rotation::None,
                    1024 * 1024, // 1MB
                    3);

  EXPECT_TRUE(output.is_open());

  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 42,
                 .function = "test_func",
                 .message = "Test message",
                 .time_point = std::chrono::system_clock::now()};

  output.write("Test log line", entry);
  output.flush();

  EXPECT_TRUE(std::filesystem::exists("test_logs/test.log"));

  // Check file content
  std::ifstream ifs("test_logs/test.log");
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  EXPECT_FALSE(content.empty());
  EXPECT_TRUE(content.find("Test log line") != std::string::npos);
}

TEST_F(LoggerTest, FileOutputRotationBySize) {
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
  EXPECT_TRUE(std::filesystem::exists("test_logs/rotate.log"));
}

TEST_F(LoggerTest, FileOutputRotationByTime) {
  std::filesystem::create_directories("test_logs");

  FileOutput output("test_logs/time_rotate.log", FileOutput::Rotation::Time,
                    1024 * 1024, // 1MB
                    3, FileOutput::RotationInterval::Hourly);

  EXPECT_TRUE(output.is_open());
  // Actual time-based rotation would require waiting, we just test setup
}

TEST_F(LoggerTest, FileOutputGetCurrentPath) {
  std::filesystem::create_directories("test_logs");

  FileOutput output("test_logs/path_test.log");

  auto path = output.current_path();
  EXPECT_EQ(path, "test_logs/path_test.log");
}

// ============================================================================
// MultiOutput Tests
// ============================================================================

TEST_F(LoggerTest, MultiOutputBasic) {
  auto multi_output = std::make_unique<MultiOutput>();

  multi_output->add_output(std::make_unique<ConsoleOutput>());

  EXPECT_EQ(multi_output->output_count(), 1);

  LogEntry entry{.level = LogLevel::Info,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 42,
                 .function = "test_func",
                 .message = "Test message",
                 .time_point = std::chrono::system_clock::now()};

  multi_output->write("Test", entry);
  multi_output->flush();

  EXPECT_TRUE(multi_output->is_open());
}

TEST_F(LoggerTest, MultiOutputMultipleDestinations) {
  auto multi_output = std::make_unique<MultiOutput>();

  multi_output->add_output(std::make_unique<ConsoleOutput>());
  multi_output->add_output(std::make_unique<ConsoleOutput>());

  EXPECT_EQ(multi_output->output_count(), 2);

  multi_output->remove_output(0);
  EXPECT_EQ(multi_output->output_count(), 1);

  multi_output->clear_outputs();
  EXPECT_EQ(multi_output->output_count(), 0);
  EXPECT_FALSE(multi_output->is_open());
}

// ============================================================================
// Logger with Multiple Outputs
// ============================================================================

TEST_F(LoggerTest, LoggerWithMultipleOutputs) {
  std::filesystem::create_directories("test_logs");

  Logger logger(std::make_unique<TextFormatter>(), std::make_unique<ConsoleOutput>());

  // Add file output
  logger.add_output(
      std::make_unique<FileOutput>("test_logs/multi.log", FileOutput::Rotation::None));

  logger.info("Message to both console and file");

  EXPECT_TRUE(std::filesystem::exists("test_logs/multi.log"));
}

// ============================================================================
// Global Logger Tests
// ============================================================================

TEST_F(LoggerTest, GlobalLogger) {
  auto& logger = global_logger();

  logger.set_level(LogLevel::Info);
  EXPECT_EQ(logger.level(), LogLevel::Info);

  logger.info("Global logger info");
}

TEST_F(LoggerTest, GlobalLoggerConvenienceFunctions) {
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

TEST_F(LoggerTest, LoggerFlush) {
  std::filesystem::create_directories("test_logs");

  Logger logger(std::make_unique<TextFormatter>(),
                std::make_unique<FileOutput>("test_logs/flush.log"));

  logger.info("Message before flush");
  logger.flush();

  // File should contain the message
  std::ifstream ifs("test_logs/flush.log");
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  EXPECT_TRUE(content.find("Message before flush") != std::string::npos);
}

// ============================================================================
// Formatted Logging Tests
// ============================================================================

TEST_F(LoggerTest, FormattedLogging) {
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

TEST_F(LoggerTest, LogEntryConstruction) {
  LogEntry entry{.level = LogLevel::Error,
                 .timestamp = "2023-01-01T12:00:00.000Z",
                 .file = "test.cpp",
                 .line = 100,
                 .function = "my_function",
                 .message = "Test error message",
                 .time_point = std::chrono::system_clock::now()};

  EXPECT_EQ(entry.level, LogLevel::Error);
  EXPECT_EQ(entry.timestamp, "2023-01-01T12:00:00.000Z");
  EXPECT_EQ(entry.file, "test.cpp");
  EXPECT_EQ(entry.line, 100);
  EXPECT_EQ(entry.function, "my_function");
  EXPECT_EQ(entry.message, "Test error message");
}
