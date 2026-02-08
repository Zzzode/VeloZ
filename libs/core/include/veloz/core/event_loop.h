#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <chrono>
#include <optional>

namespace veloz::core {

class EventLoop final {
public:
  EventLoop();
  ~EventLoop();

  EventLoop(const EventLoop&) = delete;
  EventLoop& operator=(const EventLoop&) = delete;

  void post(std::function<void()> task);
  void post_delayed(std::function<void()> task, std::chrono::milliseconds delay);

  void run();
  void stop();

  [[nodiscard]] bool is_running() const;
  [[nodiscard]] size_t pending_tasks() const;

private:
  struct DelayedTask {
    std::chrono::steady_clock::time_point deadline;
    std::function<void()> task;
    bool operator>(const DelayedTask& other) const {
      return deadline > other.deadline;
    }
  };

  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};

  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::queue<std::function<void()>> tasks_;
  std::priority_queue<DelayedTask, std::vector<DelayedTask>, std::greater<>> delayed_tasks_;
};

} // namespace veloz::core
