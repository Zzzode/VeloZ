#include "veloz/core/logger.h"

#include "kj/string.h"
#include "veloz/core/time.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <kj/common.h>
#include <kj/filesystem.h>
#include <kj/memory.h>
#include <kj/string-tree.h>

namespace veloz::core {

// ============================================================================
// to_string(LogLevel) Implementation
// ============================================================================

kj::StringPtr to_string(LogLevel level) {
  switch (level) {
  case LogLevel::Trace:
    return "TRACE"_kj;
  case LogLevel::Debug:
    return "DEBUG"_kj;
  case LogLevel::Info:
    return "INFO"_kj;
  case LogLevel::Warn:
    return "WARN"_kj;
  case LogLevel::Error:
    return "ERROR"_kj;
  case LogLevel::Critical:
    return "CRITICAL"_kj;
  case LogLevel::Off:
    return "OFF"_kj;
  }
  return "UNKNOWN"_kj;
}

// ============================================================================
// TextFormatter Implementation
// ============================================================================

kj::String TextFormatter::colorize(LogLevel level, kj::StringPtr text) const {
  if (!use_color_) {
    return kj::heapString(text);
  }

  kj::ConstString color_code = "\033[0m"_kjc;
  switch (level) {
  case LogLevel::Trace:
    color_code = "\033[90m"_kjc; // Gray
    break;
  case LogLevel::Debug:
    color_code = "\033[36m"_kjc; // Cyan
    break;
  case LogLevel::Info:
    color_code = "\033[32m"_kjc; // Green
    break;
  case LogLevel::Warn:
    color_code = "\033[33m"_kjc; // Yellow
    break;
  case LogLevel::Error:
    color_code = "\033[31m"_kjc; // Red
    break;
  case LogLevel::Critical:
    color_code = "\033[35m"_kjc; // Magenta
    break;
  default:
    color_code = "\033[0m"_kjc;
  }

  kj::ConstString reset = "\033[0m"_kjc;
  return kj::str(color_code, text, reset);
}

kj::String TextFormatter::format(const LogEntry& entry) const {
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

  char timestamp[64];
  std::snprintf(timestamp, sizeof(timestamp), "[%04d-%02d-%02d %02d:%02d:%02d.%03d]",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                static_cast<int>(ms.count()));

  kj::String label = kj::str("["_kj, to_string(entry.level), "]"_kj);
  kj::String levelPart = use_color_ ? colorize(entry.level, label.asPtr()) : kj::mv(label);

  kj::String location = include_function_
                            ? kj::str(entry.file, ":"_kj, entry.line, " "_kj, entry.function)
                            : kj::str(entry.file, ":"_kj, entry.line);

  return kj::str(timestamp, " "_kj, levelPart, " "_kj, location, " - "_kj, entry.message);
}

// ============================================================================
// JsonFormatter Implementation
// ============================================================================

kj::String JsonFormatter::escape_json(kj::StringPtr s) const {
  kj::Vector<char> result;
  result.reserve(s.size() * 2);

  auto appendLiteral = [&result](const char* lit) {
    for (const char* p = lit; *p != '\0'; ++p) {
      result.add(*p);
    }
  };

  for (char c : s) {
    switch (c) {
    case '"':
      appendLiteral("\\\"");
      break;
    case '\\':
      appendLiteral("\\\\");
      break;
    case '\b':
      appendLiteral("\\b");
      break;
    case '\f':
      appendLiteral("\\f");
      break;
    case '\n':
      appendLiteral("\\n");
      break;
    case '\r':
      appendLiteral("\\r");
      break;
    case '\t':
      appendLiteral("\\t");
      break;
    default:
      if (c < 0x20) {
        char buf[7];
        std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
        for (int i = 0; i < 6; ++i) {
          result.add(buf[i]);
        }
      } else {
        result.add(c);
      }
    }
  }

  auto out = kj::heapString(result.size());
  if (result.size() > 0) {
    std::memcpy(out.begin(), result.begin(), result.size());
  }
  return out;
}

kj::String JsonFormatter::format(const LogEntry& entry) const {
  kj::StringPtr indent = pretty_ ? "  "_kj : ""_kj;
  kj::StringPtr newline = pretty_ ? "\n"_kj : ""_kj;

  kj::Vector<kj::StringTree> parts;
  parts.add(kj::strTree("{"_kj, newline));
  parts.add(kj::strTree(indent, "\"timestamp\": \""_kj, escape_json(entry.timestamp.asPtr()),
                        "\","_kj, newline));
  parts.add(kj::strTree(indent, "\"level\": \""_kj, to_string(entry.level), "\","_kj, newline));
  parts.add(
      kj::strTree(indent, "\"file\": \""_kj, escape_json(entry.file.asPtr()), "\","_kj, newline));
  parts.add(kj::strTree(indent, "\"line\": "_kj, static_cast<int>(entry.line), "\","_kj, newline));
  parts.add(kj::strTree(indent, "\"function\": \""_kj, escape_json(entry.function.asPtr()),
                        "\","_kj, newline));
  parts.add(kj::strTree(indent, "\"message\": \""_kj, escape_json(entry.message.asPtr()), "\""_kj,
                        newline));
  parts.add(kj::strTree("}"_kj));

  return kj::StringTree(parts.releaseAsArray(), ""_kj).flatten();
}

// ============================================================================
// ConsoleOutput Implementation
// ============================================================================

void ConsoleOutput::write(kj::StringPtr formatted, const LogEntry& entry) {
  std::ostream* out = &std::cout;

  if (use_stderr_) {
    out = &std::cerr;
  } else {
    // Error and Critical messages go to stderr
    if (entry.level == LogLevel::Error || entry.level == LogLevel::Critical) {
      out = &std::cerr;
    }
  }

  out->write(formatted.begin(), formatted.size());
  *out << std::endl;
}

void ConsoleOutput::flush() {
  std::cout.flush();
  std::cerr.flush();
}

// ============================================================================
// FileOutput Implementation
// ============================================================================

namespace {

kj::Path getRotatedPath(const kj::Path& basePath, kj::StringPtr suffix, size_t index) {
  auto parent = basePath.parent();
  const auto& basename = basePath.basename()[0];
  kj::StringPtr name = basename;

  kj::String stem;
  kj::String ext;
  KJ_IF_SOME(dotPos, name.findLast('.')) {
    if (dotPos == 0) {
      stem = kj::heapString(name);
      ext = kj::str(""_kj);
    } else {
      stem = kj::heapString(name.cStr(), dotPos);
      ext = kj::heapString(name.cStr() + dotPos, name.size() - dotPos);
    }
  }
  else {
    stem = kj::heapString(name);
    ext = kj::str(""_kj);
  }

  kj::String rotatedNameKj = index == 0 ? kj::str(stem, "_"_kj, suffix, ext)
                                        : kj::str(stem, "_"_kj, suffix, "."_kj, index, ext);
  if (parent.size() == 0) {
    return kj::Path(kj::mv(rotatedNameKj));
  }
  return parent.append(kj::mv(rotatedNameKj));
}

} // namespace

FileOutput::FileOutput(const kj::Path& file_path, Rotation rotation, size_t max_size,
                       size_t max_files, RotationInterval interval)
    : guarded_(FileOutputState(file_path, max_size, rotation, interval)), max_files_(max_files) {

  // Open the initial file
  auto lock = guarded_.lockExclusive();
  auto pathStr = lock->file_path.toString();
  lock->file_stream.open(pathStr.cStr(), std::ios::out | std::ios::app);
  if (!lock->file_stream.is_open()) {
    auto msg = kj::str("Failed to open log file: "_kj, pathStr);
    throw std::runtime_error(msg.cStr());
  }

  // Get current file size
  lock->file_stream.seekp(0, std::ios::end);
  lock->current_size = lock->file_stream.tellp();
}

FileOutput::~FileOutput() noexcept {
  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               auto lock = guarded_.lockExclusive();
               if (lock->file_stream.is_open()) {
                 lock->file_stream.close();
               }
             })) {}
}

void FileOutput::write(kj::StringPtr formatted, const LogEntry& /* entry */) {
  auto lock = guarded_.lockExclusive();
  writeImpl(*lock, formatted);
}

void FileOutput::writeImpl(FileOutputState& state, kj::StringPtr formatted) {
  // Rotation is checked in the check_rotation() method, not here
  // since state is already locked/unlocked

  if (!state.file_stream.is_open()) {
    return;
  }

  state.file_stream.write(formatted.begin(), formatted.size());
  state.file_stream << std::endl;
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
    if (should_rotate_by_time(*lock)) {
      should_rotate = true;
    }
  }

  if (should_rotate) {
    perform_rotation(*lock);
  }
}

bool FileOutput::should_rotate_by_time(const FileOutputState& state) const {
  auto now = std::chrono::system_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::hours>(now - state.last_rotation);

  switch (state.interval) {
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

kj::String FileOutput::get_rotation_suffix() const {
  auto now = std::chrono::system_clock::now();
  auto time_t_sec = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &time_t_sec);
#else
  localtime_r(&time_t_sec, &tm);
#endif

  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1,
                tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  return kj::str(buf);
}

kj::Path FileOutput::get_rotated_path(size_t index) const {
  auto lock = guarded_.lockExclusive();
  auto suffix = get_rotation_suffix();
  return getRotatedPath(lock->file_path, suffix.asPtr(), index);
}

void FileOutput::perform_rotation(FileOutputState& state) {
  state.file_stream.close();
  auto suffix = get_rotation_suffix();

  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getRoot();

  // Rename existing backup files
  for (int i = static_cast<int>(max_files_) - 1; i >= 1; --i) {
    auto oldPath = getRotatedPath(state.file_path, suffix.asPtr(), static_cast<size_t>(i - 1));
    auto newPath = getRotatedPath(state.file_path, suffix.asPtr(), static_cast<size_t>(i));

    if (root.exists(oldPath)) {
      if (root.exists(newPath)) {
        root.remove(newPath);
      }
      root.transfer(newPath, kj::WriteMode::CREATE, root, oldPath, kj::TransferMode::MOVE);
    }
  }

  // Rotate current file
  auto rotatedPath = getRotatedPath(state.file_path, suffix.asPtr(), 0);
  if (root.exists(rotatedPath)) {
    root.remove(rotatedPath);
  }
  root.transfer(rotatedPath, kj::WriteMode::CREATE, root, state.file_path, kj::TransferMode::MOVE);

  // Open new file
  auto pathStr = state.file_path.toString();
  state.file_stream.open(pathStr.cStr(), std::ios::out | std::ios::app);
  if (!state.file_stream.is_open()) {
    auto msg = kj::str("Failed to open new log file after rotation: "_kj, pathStr);
    throw std::runtime_error(msg.cStr());
  }

  state.current_size = 0;
  state.last_rotation = std::chrono::system_clock::now();

  // Remove old backup files
  for (size_t i = max_files_; i <= max_files_ + 10; ++i) {
    auto path = getRotatedPath(state.file_path, suffix.asPtr(), i);
    if (root.exists(path)) {
      root.remove(path);
    }
  }
}

void FileOutput::rotate() {
  auto lock = guarded_.lockExclusive();
  perform_rotation(*lock);
}

kj::Path FileOutput::current_path() const {
  return guarded_.lockExclusive()->file_path.clone();
}

// ============================================================================
// MultiOutput Implementation
// ============================================================================

void MultiOutput::add_output(kj::Own<LogOutput> output) {
  auto lock = guarded_.lockExclusive();
  lock->outputs.add(kj::mv(output));
}

void MultiOutput::remove_output(size_t index) {
  auto lock = guarded_.lockExclusive();
  if (index < lock->outputs.size()) {
    for (size_t i = index + 1; i < lock->outputs.size(); ++i) {
      lock->outputs[i - 1] = kj::mv(lock->outputs[i]);
    }
    lock->outputs.removeLast();
  }
}

void MultiOutput::clear_outputs() {
  auto lock = guarded_.lockExclusive();
  lock->outputs.clear();
}

void MultiOutput::write(kj::StringPtr formatted, const LogEntry& entry) {
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

Logger::Logger(kj::Own<LogFormatter> formatter, kj::Own<LogOutput> output)
    : guarded_(LoggerState(kj::mv(formatter), kj::mv(output))) {}

Logger::~Logger() = default;

void Logger::set_level(LogLevel level) {
  auto lock = guarded_.lockExclusive();
  lock->level = level;
}

LogLevel Logger::level() const {
  return guarded_.lockExclusive()->level;
}

void Logger::set_formatter(kj::Own<LogFormatter> formatter) {
  auto lock = guarded_.lockExclusive();
  lock->formatter = kj::mv(formatter);
}

void Logger::set_output(kj::Own<LogOutput> output) {
  auto lock = guarded_.lockExclusive();
  lock->multi_output = kj::heap<MultiOutput>();
  lock->multi_output->add_output(kj::mv(output));
}

void Logger::add_output(kj::Own<LogOutput> output) {
  auto lock = guarded_.lockExclusive();
  lock->multi_output->add_output(kj::mv(output));
}

void Logger::log(LogLevel level, kj::StringPtr message, const std::source_location& location) {
  auto lock = guarded_.lockExclusive();
  if (lock->level == LogLevel::Off || static_cast<int>(level) < static_cast<int>(lock->level)) {
    return;
  }

  // Extract file name
  auto filePath = kj::StringPtr(location.file_name());
  const char* fileBegin = filePath.begin();
  const char* fileEnd = filePath.end();
  const char* base = fileEnd;
  while (base > fileBegin && base[-1] != '/' && base[-1] != '\\') {
    --base;
  }
  auto file = kj::heapString(base, static_cast<size_t>(fileEnd - base));

  // Create log entry
  auto now = std::chrono::system_clock::now();
  auto timestamp = now_utc_iso8601();
  LogEntry entry{.level = level,
                 .timestamp = kj::mv(timestamp),
                 .file = kj::mv(file),
                 .line = static_cast<int_least32_t>(location.line()),
                 .function = kj::heapString(location.function_name()),
                 .message = kj::heapString(message),
                 .time_point = now};

  // Format and write
  auto formatted = lock->formatter->format(entry);
  lock->multi_output->write(formatted.asPtr(), entry);
}

void Logger::trace(kj::StringPtr message, const std::source_location& location) {
  log(LogLevel::Trace, message, location);
}

void Logger::debug(kj::StringPtr message, const std::source_location& location) {
  log(LogLevel::Debug, message, location);
}

void Logger::info(kj::StringPtr message, const std::source_location& location) {
  log(LogLevel::Info, message, location);
}

void Logger::warn(kj::StringPtr message, const std::source_location& location) {
  log(LogLevel::Warn, message, location);
}

void Logger::error(kj::StringPtr message, const std::source_location& location) {
  log(LogLevel::Error, message, location);
}

void Logger::critical(kj::StringPtr message, const std::source_location& location) {
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
    return *logger; // Dereference kj::Own<Logger> to get Logger reference
  }
  // Create new logger and store it
  auto newLogger =
      kj::heap<Logger>(kj::heap<TextFormatter>(false, false), kj::heap<ConsoleOutput>());
  Logger& ref = *newLogger;
  lock->logger = kj::mv(newLogger);
  return ref;
}

} // namespace veloz::core
