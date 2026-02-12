#include "veloz/core/event_loop.h"

#include <format>

namespace veloz::core {

std::string_view to_string(EventPriority priority) {
  switch (priority) {
  case EventPriority::Low:
    return "Low";
  case EventPriority::Normal:
    return "Normal";
  case EventPriority::High:
    return "High";
  case EventPriority::Critical:
    return "Critical";
  }
  return "Unknown";
}

EventLoop::EventLoop() = default;

EventLoop::~EventLoop() {
  stop();
}

void EventLoop::post(std::function<void()> task) {
  post(std::move(task), EventPriority::Normal);
}

void EventLoop::post_delayed(std::function<void()> task, std::chrono::milliseconds delay) {
  post_delayed(std::move(task), delay, EventPriority::Normal);
}

void EventLoop::post(std::function<void()> task, EventPriority priority) {
  post_with_tags(std::move(task), priority, {});
}

void EventLoop::post_with_tags(std::function<void()> task, std::vector<EventTag> tags) {
  post_with_tags(std::move(task), EventPriority::Normal, std::move(tags));
}

void EventLoop::post_with_tags(std::function<void()> task, EventPriority priority,
                               std::vector<EventTag> tags) {
  Task t{.task = std::move(task),
         .priority = priority,
         .tags = std::move(tags),
         .enqueue_time = std::chrono::steady_clock::now()};
  {
    std::scoped_lock lock(mu_);
    tasks_.push(std::move(t));
    stats_.total_events++;
    stats_.events_by_priority[static_cast<uint8_t>(priority)]++;
  }
  cv_.notify_one();
}

void EventLoop::post_delayed(std::function<void()> task, std::chrono::milliseconds delay,
                             EventPriority priority) {
  post_delayed(std::move(task), delay, priority, {});
}

void EventLoop::post_delayed(std::function<void()> task, std::chrono::milliseconds delay,
                             std::vector<EventTag> tags) {
  post_delayed(std::move(task), delay, EventPriority::Normal, std::move(tags));
}

void EventLoop::post_delayed(std::function<void()> task, std::chrono::milliseconds delay,
                             EventPriority priority, std::vector<EventTag> tags) {
  auto deadline = std::chrono::steady_clock::now() + delay;
  Task t{.task = std::move(task),
         .priority = priority,
         .tags = std::move(tags),
         .enqueue_time = std::chrono::steady_clock::now()};
  {
    std::scoped_lock lock(mu_);
    delayed_tasks_.push({deadline, std::move(t)});
    stats_.total_delayed_events++;
  }
  cv_.notify_one();
}

bool EventLoop::should_process_task(const Task& task) {
  // Check priority-based filters
  for (const auto& [id, filter_pair] : filters_) {
    if (!active_filters_.contains(id)) {
      continue;
    }
    const auto& [filter, priority_filter] = filter_pair;
    if (priority_filter.has_value() && priority_filter.value() != task.priority) {
      continue;
    }
    if (filter(task.tags)) {
      stats_.events_filtered++;
      return false;
    }
  }

  // Check tag pattern filters
  for (const auto& [id, pattern] : tag_filters_) {
    if (!active_filters_.contains(id)) {
      continue;
    }
    for (const auto& tag : task.tags) {
      try {
        if (std::regex_search(tag, pattern)) {
          stats_.events_filtered++;
          return false;
        }
      } catch (const std::regex_error&) {
        // Invalid regex, skip this filter
      }
    }
  }

  return true;
}

void EventLoop::route_task(Task& task, std::function<void()> wrapped) {
  if (router_) {
    router_.value()(task.tags, [&task] { task.task(); });
  } else {
    wrapped();
  }
}

void EventLoop::execute_task(Task& task) {
  // Track queue wait time
  auto now = std::chrono::steady_clock::now();
  auto wait_time =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now - task.enqueue_time).count();
  stats_.queue_wait_time_ns += static_cast<uint64_t>(wait_time);
  uint64_t current_max = stats_.max_queue_wait_time_ns.load();
  uint64_t wait_time_u = static_cast<uint64_t>(wait_time);
  while (wait_time_u > current_max &&
         !stats_.max_queue_wait_time_ns.compare_exchange_weak(current_max, wait_time_u)) {
    // Retry if another thread updated it
  }

  // Execute task with timing
  auto start = std::chrono::steady_clock::now();
  try {
    if (router_) {
      router_.value()(task.tags, [&task] { task.task(); });
    } else {
      task.task();
    }
    stats_.events_processed++;
  } catch (...) {
    stats_.events_failed++;
  }
  auto end = std::chrono::steady_clock::now();

  // Track processing time
  auto processing_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  stats_.processing_time_ns += static_cast<uint64_t>(processing_time);
  current_max = stats_.max_processing_time_ns.load();
  while (static_cast<uint64_t>(processing_time) > current_max &&
         !stats_.max_processing_time_ns.compare_exchange_weak(
             current_max, static_cast<uint64_t>(processing_time))) {
    // Retry if another thread updated it
  }
}

void EventLoop::run() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    return;
  }

  stop_requested_.store(false);
  for (;;) {
    std::optional<Task> task;
    {
      std::unique_lock lock(mu_);

      auto now = std::chrono::steady_clock::now();

      // Check delayed tasks
      while (!delayed_tasks_.empty() && delayed_tasks_.top().deadline <= now) {
        Task delayed_task = std::move(delayed_tasks_.top().task);
        delayed_tasks_.pop();
        if (should_process_task(delayed_task)) {
          tasks_.push(std::move(delayed_task));
        }
      }

      // Get next task
      if (!tasks_.empty()) {
        task = std::move(const_cast<Task&>(tasks_.top()));
        tasks_.pop();

        // Apply filtering
        if (!should_process_task(*task)) {
          continue;
        }
      } else if (!delayed_tasks_.empty()) {
        // Wait for delayed task
        cv_.wait_until(lock, delayed_tasks_.top().deadline,
                       [&] { return stop_requested_.load() || !tasks_.empty(); });
        continue;
      } else if (stop_requested_.load()) {
        break;
      } else {
        cv_.wait(lock, [&] { return stop_requested_.load() || !tasks_.empty(); });
        continue;
      }
    }

    if (task) {
      execute_task(*task);
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

size_t EventLoop::pending_tasks_by_priority(EventPriority priority) const {
  std::scoped_lock lock(mu_);
  size_t count = 0;
  // Note: We can't efficiently count by priority in a priority_queue without iterating
  // This is O(n) but acceptable for status queries
  auto temp_tasks = tasks_;
  while (!temp_tasks.empty()) {
    if (temp_tasks.top().priority == priority) {
      count++;
    }
    temp_tasks.pop();
  }
  return count;
}

std::string EventLoop::stats_to_string() const {
  return std::format(R"(
EventLoop Statistics:
  Total Events: {}
  Delayed Events: {}
  Events Processed: {}
  Events Failed: {}
  Events Filtered: {}
  By Priority:
    Low: {}
    Normal: {}
    High: {}
    Critical: {}
  Processing Time: {:.3f} ms
  Max Processing Time: {:.3f} ms
  Avg Queue Wait Time: {:.3f} ms
  Max Queue Wait Time: {:.3f} ms
)",
                     stats_.total_events.load(), stats_.total_delayed_events.load(),
                     stats_.events_processed.load(), stats_.events_failed.load(),
                     stats_.events_filtered.load(), stats_.events_by_priority[0].load(),
                     stats_.events_by_priority[1].load(), stats_.events_by_priority[2].load(),
                     stats_.events_by_priority[3].load(), stats_.processing_time_ns.load() / 1e6,
                     stats_.max_processing_time_ns.load() / 1e6,
                     stats_.events_processed.load() > 0
                         ? (stats_.queue_wait_time_ns.load() / 1e6) / stats_.events_processed.load()
                         : 0.0,
                     stats_.max_queue_wait_time_ns.load() / 1e6);
}

uint64_t EventLoop::add_filter(EventFilter filter, std::optional<EventPriority> priority) {
  uint64_t id = next_filter_id_++;
  {
    std::scoped_lock lock(mu_);
    filters_[id] = {std::move(filter), priority};
    active_filters_.insert(id);
  }
  return id;
}

void EventLoop::remove_filter(uint64_t filter_id) {
  std::scoped_lock lock(mu_);
  active_filters_.erase(filter_id);
  filters_.erase(filter_id);
}

void EventLoop::clear_filters() {
  std::scoped_lock lock(mu_);
  active_filters_.clear();
  filters_.clear();
  tag_filters_.clear();
}

uint64_t EventLoop::add_tag_filter(const std::string& tag_pattern) {
  uint64_t id = next_filter_id_++;
  try {
    std::scoped_lock lock(mu_);
    tag_filters_[id] = std::regex(tag_pattern);
    active_filters_.insert(id);
  } catch (const std::regex_error& e) {
    // Remove the ID if regex creation failed
    active_filters_.erase(id);
    throw std::runtime_error(std::format("Invalid tag pattern '{}': {}", tag_pattern, e.what()));
  }
  return id;
}

void EventLoop::remove_tag_filter(uint64_t filter_id) {
  std::scoped_lock lock(mu_);
  active_filters_.erase(filter_id);
  tag_filters_.erase(filter_id);
}

void EventLoop::set_router(EventRouter router) {
  std::scoped_lock lock(mu_);
  router_ = std::move(router);
}

void EventLoop::clear_router() {
  std::scoped_lock lock(mu_);
  router_ = std::nullopt;
}

} // namespace veloz::core
