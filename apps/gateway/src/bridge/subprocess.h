/**
 * @file subprocess.h
 * @brief Subprocess management for engine process communication
 *
 * Provides async subprocess spawning with pipe-based stdio communication
 * using KJ's async I/O framework.
 */

#pragma once

#include <kj/array.h>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <unistd.h>

#ifdef stdin
#undef stdin
#endif
#ifdef stdout
#undef stdout
#endif

namespace veloz::gateway::bridge {

/**
 * @brief Result of subprocess exit
 */
struct ExitResult {
  int exit_code;
  kj::Maybe<kj::String> error_message;
};

/**
 * @brief Handle for managing a subprocess with async stdio
 *
 * Provides:
 * - Process spawning via fork/exec
 * - Async stdin/stdout pipes
 * - Process lifecycle management (wait, kill)
 *
 * Uses Unix pipes for communication with the subprocess.
 */
class SubprocessHandle {
public:
  /**
   * @brief Create a subprocess handle
   */
  explicit SubprocessHandle(kj::AsyncIoContext& io);
  ~SubprocessHandle();

  // Non-copyable, non-movable
  SubprocessHandle(const SubprocessHandle&) = delete;
  SubprocessHandle& operator=(const SubprocessHandle&) = delete;
  SubprocessHandle(SubprocessHandle&&) = delete;
  SubprocessHandle& operator=(SubprocessHandle&&) = delete;

  /**
   * @brief Spawn a subprocess
   *
   * @param command Path to executable
   * @param args Arguments to pass (not including argv[0])
   * @return Promise that resolves when process is spawned
   * @throws kj::Exception on failure
   */
  kj::Promise<void> spawn(kj::StringPtr command, kj::Array<kj::StringPtr> args);

  /**
   * @brief Get stdin stream for writing
   *
   * Valid after successful spawn()
   */
  kj::AsyncOutputStream& stdin();

  /**
   * @brief Get stdout stream for reading
   *
   * Valid after successful spawn()
   */
  kj::AsyncInputStream& stdout();

  /**
   * @brief Wait for subprocess to exit
   *
   * @return Promise that resolves with exit code
   */
  kj::Promise<ExitResult> waitExit();

  /**
   * @brief Kill the subprocess
   *
   * Sends SIGKILL to the subprocess.
   */
  void kill();

  /**
   * @brief Check if process is running
   */
  [[nodiscard]] bool isRunning() const noexcept {
    return running_;
  }

  /**
   * @brief Get process PID
   */
  [[nodiscard]] pid_t pid() const noexcept {
    return pid_;
  }

private:
  kj::AsyncIoContext& io_;

  // Process state
  pid_t pid_{-1};
  bool running_{false};

  // Pipe file descriptors
  int stdin_pipe_[2]{-1, -1};  // [0] = read end, [1] = write end
  int stdout_pipe_[2]{-1, -1}; // [0] = read end, [1] = write end

  // KJ async streams (created after spawn)
  kj::Maybe<kj::Own<kj::AsyncOutputStream>> stdin_stream_;
  kj::Maybe<kj::Own<kj::AsyncInputStream>> stdout_stream_;

  // Cleanup resources
  void closePipes();
};

} // namespace veloz::gateway::bridge
