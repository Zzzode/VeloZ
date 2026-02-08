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

void EventLoop::post_delayed(std::function<void()> task, std::chrono::milliseconds delay) {
  auto deadline = std::chrono::steady_clock::now() + delay;
  {
    std::scoped_lock lock(mu_);
    delayed_tasks_.push({deadline, std::move(task)});
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

      auto now = std::chrono::steady_clock::now();
      if (!delayed_tasks_.empty()) {
        if (delayed_tasks_.top().deadline <= now) {
          task = std::move(delayed_tasks_.top().task);
          delayed_tasks_.pop();
        } else if (tasks_.empty()) {
          cv_.wait_until(lock, delayed_tasks_.top().deadline,
                       [&] { return stop_requested_.load() || !tasks_.empty() ||
                               (!delayed_tasks_.empty() && delayed_tasks_.top().deadline <= std::chrono::steady_clock::now()); });
        }
      }

      if (!task && !tasks_.empty()) {
        task = std::move(tasks_.front());
        tasks_.pop();
      }

      if (!task && stop_requested_.load() && delayed_tasks_.empty()) {
        break;
      }
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

size_t EventLoop::pending_tasks() const {
  std::scoped_lock lock(mu_);
  return tasks_.size() + delayed_tasks_.size();
}

} // namespace veloz::core
