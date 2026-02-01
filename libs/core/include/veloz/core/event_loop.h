#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace veloz::core {

class EventLoop final {
public:
  EventLoop();
  ~EventLoop();

  EventLoop(const EventLoop&) = delete;
  EventLoop& operator=(const EventLoop&) = delete;

  void post(std::function<void()> task);

  void run();
  void stop();

  [[nodiscard]] bool is_running() const;

private:
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};

  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::queue<std::function<void()>> tasks_;
};

} // namespace veloz::core
