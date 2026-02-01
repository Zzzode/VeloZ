#include "veloz/core/event_loop.h"

namespace veloz::core {

EventLoop::EventLoop() = default;

EventLoop::~EventLoop() {
  stop();
}

void EventLoop::post(std::function<void()> task) {
  {
    std::scoped_lock lock(mu_);
    tasks_.push(std::move(task));
  }
  cv_.notify_one();
}

void EventLoop::run() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    return;
  }

  stop_requested_.store(false);
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock lock(mu_);
      cv_.wait(lock, [&] { return stop_requested_.load() || !tasks_.empty(); });

      if (stop_requested_.load() && tasks_.empty()) {
        break;
      }

      task = std::move(tasks_.front());
      tasks_.pop();
    }
    if (task) {
      task();
    }
  }

  running_.store(false);
}

void EventLoop::stop() {
  if (!running_.load()) {
    stop_requested_.store(true);
    cv_.notify_all();
    return;
  }
  stop_requested_.store(true);
  cv_.notify_all();
}

bool EventLoop::is_running() const {
  return running_.load();
}

} // namespace veloz::core
