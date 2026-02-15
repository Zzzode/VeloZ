#include "veloz/core/logger.h"

#include "veloz/core/time.h"

#include <algorithm>
#include <cstdarg>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <kj/common.h>
#include <kj/memory.h>
#include <sstream>

namespace veloz::core {

// ============================================================================
// to_string(LogLevel) Implementation
// ============================================================================

std::string_view to_string(LogLevel level) {
  switch (level) {
  case LogLevel::Trace:
    return "TRACE";
  case LogLevel::Debug:
    return "DEBUG";
  case LogLevel::Info:
    return "INFO";
  case LogLevel::Warn:
    return "WARN";
  case LogLevel::Error:
    return "ERROR";
  case LogLevel::Critical:
    return "CRITICAL";
  case LogLevel::Off:
    return "OFF";
  }
  return "UNKNOWN";
}

// ============================================================================
// TextFormatter Implementation
// ============================================================================

std::string TextFormatter::colorize(LogLevel level, std::string_view text) const {
  if (!use_color_) {
    return std::string(text);
  }

  const char* color_code = nullptr;
  switch (level) {
  case LogLevel::Trace:
    color_code = "\033[90m"; // Gray
    break;
  case LogLevel::Debug:
    color_code = "\033[36m"; // Cyan
    break;
  case LogLevel::Info:
    color_code = "\033[32m"; // Green
    break;
  case LogLevel::Warn:
    color_code = "\033[33m"; // Yellow
    break;
  case LogLevel::Error:
    color_code = "\033[31m"; // Red
    break;
  case LogLevel::Critical:
    color_code = "\033[35m"; // Magenta
    break;
  default:
    color_code = "\033[0m";
  }

  const char* reset = "\033[0m";
  return std::format("{}{}{}", color_code, text, reset);
}

std::string TextFormatter::format(const LogEntry& entry) const {
  std::ostringstream oss;

  // Timestamp with milliseconds
  auto time_t_sec = std::chrono::system_clock::to_time_t(entry.time_point);
  auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(entry.time_point.time_since_epoch()) %
      1000;
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &time_t_sec);
#else
  localtime_r(&time_t_sec, &tm);
#endif

  oss << std::format("[{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}]", tm.tm_year + 1900,
                     tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ms.count());

  // Level
  std::string level_str = std::string(to_string(entry.level));
  if (use_color_) {
    oss << " " << colorize(entry.level, std::format("[{}]", level_str));
  } else {
    oss << " " << std::format("[{}]", level_str);
  }

  // Source location
  std::string file = entry.file;
  size_t last_sep = file.find_last_of("/\\");
  if (last_sep != std::string::npos) {
    file = file.substr(last_sep + 1);
  }

  if (include_function_) {
    oss << " " << file << ":" << entry.line << " " << entry.function;
  } else {
    oss << " " << file << ":" << entry.line;
  }

  // Message
  oss << " - " << entry.message;

  return oss.str();
}

// ============================================================================
// JsonFormatter Implementation
// ============================================================================

std::string JsonFormatter::escape_json(std::string_view s) const {
  std::string result;
  result.reserve(s.size() * 2);

  for (char c : s) {
    switch (c) {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      if (c < 0x20) {
        result += std::format("\\u{:04x}", static_cast<unsigned char>(c));
      } else {
        result += c;
      }
    }
  }

  return result;
}

std::string JsonFormatter::format(const LogEntry& entry) const {
  std::ostringstream oss;
  std::string indent = pretty_ ? "  " : "";
  std::string newline = pretty_ ? "\n" : "";

  oss << "{" << newline;
  oss << indent << "\"timestamp\": \"" << escape_json(entry.timestamp) << "\"," << newline;
  oss << indent << "\"level\": \"" << to_string(entry.level) << "\"," << newline;
  oss << indent << "\"file\": \"" << escape_json(entry.file) << "\"," << newline;
  oss << indent << "\"line\": " << static_cast<int>(entry.line) << "," << newline;
  oss << indent << "\"function\": \"" << escape_json(entry.function) << "\"," << newline;
  oss << indent << "\"message\": \"" << escape_json(entry.message) << "\"" << newline;
  oss << "}";

  return oss.str();
}

// ============================================================================
// ConsoleOutput Implementation
// ============================================================================

void ConsoleOutput::write(const std::string& formatted, const LogEntry& entry) {
  std::ostream* out = &std::cout;

  if (use_stderr_) {
    out = &std::cerr;
  } else {
    // Error and Critical messages go to stderr
    if (entry.level == LogLevel::Error || entry.level == LogLevel::Critical) {
      out = &std::cerr;
    }
  }

  *out << formatted << std::endl;
}

void ConsoleOutput::flush() {
  std::cout.flush();
  std::cerr.flush();
}

// ============================================================================
// FileOutput Implementation
// ============================================================================

FileOutput::FileOutput(const std::filesystem::path& file_path, Rotation rotation, size_t max_size,
                       size_t max_files, RotationInterval interval)
    : guarded_(FileOutputState(file_path, max_size, rotation, interval)), max_files_(max_files) {

  // Open the initial file
  auto lock = guarded_.lockExclusive();
  lock->file_stream.open(lock->file_path, std::ios::out | std::ios::app);
  if (!lock->file_stream.is_open()) {
    throw std::runtime_error("Failed to open log file: " + lock->file_path.string());
  }

  // Get current file size
  lock->file_stream.seekp(0, std::ios::end);
  lock->current_size = lock->file_stream.tellp();
}

FileOutput::~FileOutput() {
  auto lock = guarded_.lockExclusive();
  if (lock->file_stream.is_open()) {
    lock->file_stream.close();
  }
}

void FileOutput::write(const std::string& formatted, const LogEntry& /* entry */) {
  auto lock = guarded_.lockExclusive();
  writeImpl(*lock, formatted);
}

void FileOutput::writeImpl(FileOutputState& state, const std::string& formatted) {
  // Rotation is checked in the check_rotation() method, not here
  // since state is already locked/unlocked

  if (!state.file_stream.is_open()) {
    return;
  }

  state.file_stream << formatted << std::endl;
  state.current_size += formatted.size() + 1; // +1 for newline
}

void FileOutput::flush() {
  auto lock = guarded_.lockExclusive();
  if (lock->file_stream.is_open()) {
    lock->file_stream.flush();
  }
}

bool FileOutput::is_open() const {
  return guarded_.lockExclusive()->file_stream.is_open();
}

void FileOutput::check_rotation() {
  auto lock = guarded_.lockExclusive();
  bool should_rotate = false;

  // Check size-based rotation
  if (lock->rotation == Rotation::Size || lock->rotation == Rotation::Both) {
    if (lock->current_size >= lock->max_size) {
      should_rotate = true;
    }
  }

  // Check time-based rotation
  if ((lock->rotation == Rotation::Time || lock->rotation == Rotation::Both) && !should_rotate) {
    if (should_rotate_by_time()) {
      should_rotate = true;
    }
  }

  if (should_rotate) {
    perform_rotation();
  }
}

bool FileOutput::should_rotate_by_time() const {
  auto lock = guarded_.lockExclusive();
  auto now = std::chrono::system_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::hours>(now - lock->last_rotation);

  switch (lock->interval) {
  case RotationInterval::Hourly:
    return elapsed >= std::chrono::hours(1);
  case RotationInterval::Daily:
    return elapsed >= std::chrono::hours(24);
  case RotationInterval::Weekly:
    return elapsed >= std::chrono::hours(24 * 7);
  case RotationInterval::Monthly:
    return elapsed >= std::chrono::hours(24 * 30);
  }
  return false;
}

std::string FileOutput::get_rotation_suffix() const {
  auto now = std::chrono::system_clock::now();
  auto time_t_sec = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &time_t_sec);
#else
  localtime_r(&time_t_sec, &tm);
#endif

  return std::format("{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}", tm.tm_year + 1900, tm.tm_mon + 1,
                     tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

std::filesystem::path FileOutput::get_rotated_path(size_t index) const {
  auto lock = guarded_.lockExclusive();
  std::string suffix = get_rotation_suffix();

  if (index == 0) {
    return lock->file_path.parent_path() / std::format("{}_{}{}", lock->file_path.stem().string(),
                                                       suffix,
                                                       lock->file_path.extension().string());
  }

  return lock->file_path.parent_path() / std::format("{}_{}.{}{}", lock->file_path.stem().string(),
                                                     suffix, index,
                                                     lock->file_path.extension().string());
}

void FileOutput::perform_rotation() {
  auto lock = guarded_.lockExclusive();
  lock->file_stream.close();

  // Rename existing backup files
  for (int i = static_cast<int>(max_files_) - 1; i >= 1; --i) {
    auto old_path = get_rotated_path(i - 1);
    auto new_path = get_rotated_path(i);

    if (std::filesystem::exists(old_path)) {
      std::filesystem::rename(old_path, new_path);
    }
  }

  // Rotate current file
  auto rotated_path = get_rotated_path(0);
  std::filesystem::rename(lock->file_path, rotated_path);

  // Open new file
  lock->file_stream.open(lock->file_path, std::ios::out | std::ios::app);
  if (!lock->file_stream.is_open()) {
    throw std::runtime_error("Failed to open new log file after rotation: " +
                             lock->file_path.string());
  }

  lock->current_size = 0;
  lock->last_rotation = std::chrono::system_clock::now();

  // Remove old backup files
  for (size_t i = max_files_; i <= max_files_ + 10; ++i) {
    auto path = get_rotated_path(i);
    if (std::filesystem::exists(path)) {
      std::filesystem::remove(path);
    }
  }
}

void FileOutput::rotate() {
  auto lock = guarded_.lockExclusive();
  perform_rotation();
}

std::filesystem::path FileOutput::current_path() const {
  return guarded_.lockExclusive()->file_path;
}

// ============================================================================
// MultiOutput Implementation
// ============================================================================

void MultiOutput::add_output(std::unique_ptr<LogOutput> output) {
  auto lock = guarded_.lockExclusive();
  lock->outputs.push_back(kj::mv(output));
}

void MultiOutput::remove_output(size_t index) {
  auto lock = guarded_.lockExclusive();
  if (index < lock->outputs.size()) {
    lock->outputs.erase(lock->outputs.begin() + static_cast<ptrdiff_t>(index));
  }
}

void MultiOutput::clear_outputs() {
  auto lock = guarded_.lockExclusive();
  lock->outputs.clear();
}

void MultiOutput::write(const std::string& formatted, const LogEntry& entry) {
  auto lock = guarded_.lockExclusive();
  for (auto& output : lock->outputs) {
    output->write(formatted, entry);
  }
}

void MultiOutput::flush() {
  auto lock = guarded_.lockExclusive();
  for (auto& output : lock->outputs) {
    output->flush();
  }
}

bool MultiOutput::is_open() const {
  return !guarded_.lockExclusive()->outputs.empty();
}

size_t MultiOutput::output_count() const {
  return guarded_.lockExclusive()->outputs.size();
}

// ============================================================================
// Logger Implementation
// ============================================================================

Logger::Logger(std::unique_ptr<LogFormatter> formatter, std::unique_ptr<LogOutput> output)
    : guarded_(LoggerState(kj::mv(formatter), kj::mv(output))) {}

Logger::~Logger() = default;

void Logger::set_level(LogLevel level) {
  auto lock = guarded_.lockExclusive();
  lock->level = level;
}

LogLevel Logger::level() const {
  return guarded_.lockExclusive()->level;
}

void Logger::set_formatter(std::unique_ptr<LogFormatter> formatter) {
  auto lock = guarded_.lockExclusive();
  lock->formatter = kj::mv(formatter);
}

void Logger::set_output(std::unique_ptr<LogOutput> output) {
  auto lock = guarded_.lockExclusive();
  // Uses std::make_unique for polymorphic ownership pattern (kj::Own lacks release())
  lock->multi_output = std::make_unique<MultiOutput>();
  lock->multi_output->add_output(kj::mv(output));
}

void Logger::add_output(std::unique_ptr<LogOutput> output) {
  auto lock = guarded_.lockExclusive();
  lock->multi_output->add_output(kj::mv(output));
}

void Logger::log(LogLevel level, std::string_view message, const std::source_location& location) {
  auto lock = guarded_.lockExclusive();
  if (lock->level == LogLevel::Off || static_cast<int>(level) < static_cast<int>(lock->level)) {
    return;
  }

  // Extract file name
  std::string file = std::string(location.file_name());
  size_t last_sep = file.find_last_of("/\\");
  if (last_sep != std::string::npos) {
    file = file.substr(last_sep + 1);
  }

  // Create log entry
  auto now = std::chrono::system_clock::now();
  LogEntry entry{.level = level,
                 .timestamp = std::string(now_utc_iso8601()),
                 .file = kj::mv(file),
                 .line = static_cast<int_least32_t>(location.line()),
                 .function = std::string(location.function_name()),
                 .message = std::string(message),
                 .time_point = now};

  // Format and write
  std::string formatted = lock->formatter->format(entry);
  lock->multi_output->write(formatted, entry);
}

void Logger::trace(std::string_view message, const std::source_location& location) {
  log(LogLevel::Trace, message, location);
}

void Logger::debug(std::string_view message, const std::source_location& location) {
  log(LogLevel::Debug, message, location);
}

void Logger::info(std::string_view message, const std::source_location& location) {
  log(LogLevel::Info, message, location);
}

void Logger::warn(std::string_view message, const std::source_location& location) {
  log(LogLevel::Warn, message, location);
}

void Logger::error(std::string_view message, const std::source_location& location) {
  log(LogLevel::Error, message, location);
}

void Logger::critical(std::string_view message, const std::source_location& location) {
  log(LogLevel::Critical, message, location);
}

void Logger::flush() {
  auto lock = guarded_.lockExclusive();
  lock->multi_output->flush();
}

// ============================================================================
// Global Logger
// ============================================================================

// Thread-safe global logger using KJ MutexGuarded (no bare pointers)
struct GlobalLoggerState {
  kj::Maybe<kj::Own<Logger>> logger{kj::none};
};
static kj::MutexGuarded<GlobalLoggerState> g_global_logger;

Logger& global_logger() {
  auto lock = g_global_logger.lockExclusive();
  KJ_IF_SOME(logger, lock->logger) {
    return *logger;  // Dereference kj::Own<Logger> to get Logger reference
  }
  // Create new logger and store it
  // Uses std::make_unique for polymorphic ownership pattern (kj::Own lacks release())
  auto newLogger = kj::heap<Logger>(std::make_unique<TextFormatter>(false, false),
                                    std::make_unique<ConsoleOutput>());
  Logger& ref = *newLogger;
  lock->logger = kj::mv(newLogger);
  return ref;
}

} // namespace veloz::core
