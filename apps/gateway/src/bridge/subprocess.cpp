#include "subprocess.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <kj/async-io.h>
#include <kj/async-unix.h>
#include <kj/debug.h>
#include <signal.h>
#include <sys/wait.h>

namespace veloz::gateway::bridge {

// ============================================================================
// SubprocessHandle Implementation
// ============================================================================

SubprocessHandle::SubprocessHandle(kj::AsyncIoContext& io) : io_(io) {
  stdin_pipe_[0] = -1;
  stdin_pipe_[1] = -1;
  stdout_pipe_[0] = -1;
  stdout_pipe_[1] = -1;
}

SubprocessHandle::~SubprocessHandle() {
  kill();
  closePipes();
}

void SubprocessHandle::closePipes() {
  // Close all pipe file descriptors
  for (int i = 0; i < 2; ++i) {
    if (stdin_pipe_[i] >= 0) {
      ::close(stdin_pipe_[i]);
      stdin_pipe_[i] = -1;
    }
    if (stdout_pipe_[i] >= 0) {
      ::close(stdout_pipe_[i]);
      stdout_pipe_[i] = -1;
    }
  }

  stdin_stream_ = kj::none;
  stdout_stream_ = kj::none;
}

kj::Promise<void> SubprocessHandle::spawn(kj::StringPtr command, kj::Array<kj::StringPtr> args) {
  KJ_REQUIRE(!running_, "Subprocess already running");

  // Create pipes for stdin and stdout
  // stdin_pipe_[1] = write end (parent writes to this)
  // stdout_pipe_[0] = read end (parent reads from this)

  if (::pipe(stdin_pipe_) < 0) {
    KJ_FAIL_REQUIRE("Failed to create stdin pipe", std::strerror(errno));
  }

  if (::pipe(stdout_pipe_) < 0) {
    int saved_errno = errno;
    ::close(stdin_pipe_[0]);
    ::close(stdin_pipe_[1]);
    KJ_FAIL_REQUIRE("Failed to create stdout pipe", std::strerror(saved_errno));
  }

  // Fork the process
  pid_ = ::fork();

  if (pid_ < 0) {
    int saved_errno = errno;
    closePipes();
    KJ_FAIL_REQUIRE("Failed to fork process", std::strerror(saved_errno));
  }

  if (pid_ == 0) {
    // Child process

    // Redirect stdin to read end of stdin pipe
    if (::dup2(stdin_pipe_[0], STDIN_FILENO) < 0) {
      _exit(1);
    }

    // Redirect stdout to write end of stdout pipe
    if (::dup2(stdout_pipe_[1], STDOUT_FILENO) < 0) {
      _exit(1);
    }

    // Close all pipe file descriptors (we've redirected them)
    ::close(stdin_pipe_[0]);
    ::close(stdin_pipe_[1]);
    ::close(stdout_pipe_[0]);
    ::close(stdout_pipe_[1]);

    // Build argv array (command + args + nullptr)
    kj::Vector<char*> argv;
    argv.add(const_cast<char*>(command.cStr()));

    for (auto& arg : args) {
      argv.add(const_cast<char*>(arg.cStr()));
    }

    argv.add(nullptr);

    // Execute the command
    ::execvp(command.cStr(), argv.begin());

    // If execvp returns, it failed
    _exit(1);
  }

  // Parent process

  // Close the ends we don't use
  ::close(stdin_pipe_[0]);  // Close read end of stdin pipe
  ::close(stdout_pipe_[1]); // Close write end of stdout pipe

  stdin_pipe_[0] = -1;
  stdout_pipe_[1] = -1;

  // Set non-blocking mode on the pipes
  int flags = ::fcntl(stdin_pipe_[1], F_GETFL, 0);
  if (flags < 0 || ::fcntl(stdin_pipe_[1], F_SETFL, flags | O_NONBLOCK) < 0) {
    int saved_errno = errno;
    kill();
    closePipes();
    KJ_FAIL_REQUIRE("Failed to set stdin pipe non-blocking", std::strerror(saved_errno));
  }

  flags = ::fcntl(stdout_pipe_[0], F_GETFL, 0);
  if (flags < 0 || ::fcntl(stdout_pipe_[0], F_SETFL, flags | O_NONBLOCK) < 0) {
    int saved_errno = errno;
    kill();
    closePipes();
    KJ_FAIL_REQUIRE("Failed to set stdout pipe non-blocking", std::strerror(saved_errno));
  }

  // Create KJ async streams from the pipes
  stdin_stream_ = io_.lowLevelProvider->wrapOutputFd(stdin_pipe_[1],
                                                     kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP);

  stdout_stream_ = io_.lowLevelProvider->wrapInputFd(stdout_pipe_[0],
                                                     kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP);

  // Mark pipes as taken ownership
  stdin_pipe_[1] = -1;
  stdout_pipe_[0] = -1;

  running_ = true;

  return kj::READY_NOW;
}

kj::AsyncOutputStream& SubprocessHandle::stdin() {
  KJ_REQUIRE(running_, "Subprocess not running");
  KJ_IF_SOME(stream, stdin_stream_) {
    return *stream;
  }
  else {
    KJ_FAIL_REQUIRE("stdin stream not available");
  }
}

kj::AsyncInputStream& SubprocessHandle::stdout() {
  KJ_REQUIRE(running_, "Subprocess not running");
  KJ_IF_SOME(stream, stdout_stream_) {
    return *stream;
  }
  else {
    KJ_FAIL_REQUIRE("stdout stream not available");
  }
}

kj::Promise<ExitResult> SubprocessHandle::waitExit() {
  KJ_REQUIRE(pid_ > 0, "Process not spawned");

  // Use KJ's async wait for child process
  return io_.unixEventPort.onSignal(SIGCHLD).then([this](auto&&) -> kj::Promise<ExitResult> {
    // Check if our specific child process has exited
    int status;
    pid_t result = ::waitpid(pid_, &status, WNOHANG);

    if (result < 0) {
      // Error
      ExitResult exit_result;
      exit_result.exit_code = -1;
      exit_result.error_message = kj::str("waitpid failed: ", std::strerror(errno));
      running_ = false;
      return exit_result;
    } else if (result == 0) {
      // Child hasn't exited yet, wait again
      return waitExit();
    } else {
      // Child exited
      ExitResult exit_result;

      if (WIFEXITED(status)) {
        exit_result.exit_code = WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        exit_result.exit_code = 128 + WTERMSIG(status);
        exit_result.error_message = kj::str("Process killed by signal ", WTERMSIG(status));
      } else {
        exit_result.exit_code = -1;
        exit_result.error_message = kj::str("Unknown exit status");
      }

      running_ = false;
      return exit_result;
    }
  });
}

void SubprocessHandle::kill() {
  if (running_ && pid_ > 0) {
    ::kill(pid_, SIGKILL);
    running_ = false;

    // Wait for process to avoid zombie
    int status;
    ::waitpid(pid_, &status, 0);
  }
}

} // namespace veloz::gateway::bridge
