#include "veloz/core/event_loop.h"

// =======================================================================================
// std Library Usage Justification (see event_loop.h for full documentation)
// =======================================================================================
// - std::chrono::steady_clock: Monotonic time for task queue wait time measurement
// - std::iomanip/std::sstream: String formatting (kj::str() lacks width specifiers)
// - std::regex: Pattern matching for event tag filters (KJ lacks regex support)
// =======================================================================================

#include <chrono> // std::chrono::steady_clock - KJ time types don't provide steady_clock equivalent
#include <iomanip> // std::setprecision - kj::str() lacks width/precision specifiers
#include <kj/async-io.h>
#include <kj/async.h>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/string.h>
#include <kj/time.h>
#include <sstream> // std::ostringstream - kj::str() lacks width/precision specifiers
#include <thread>  // std::this_thread::sleep_for - for startup synchronization only

namespace veloz::core {

kj::StringPtr to_string(EventPriority priority) {
  switch (priority) {
  case EventPriority::Low:
    return "Low"_kj;
  case EventPriority::Normal:
    return "Normal"_kj;
  case EventPriority::High:
    return "High"_kj;
  case EventPriority::Critical:
    return "Critical"_kj;
  }
  return "Unknown"_kj;
}

// TaskSetErrorHandler implementation
void EventLoop::TaskSetErrorHandler::taskFailed(kj::Exception&& exception) {
  stats_.events_failed++;
  KJ_LOG(ERROR, "Task failed in EventLoop", exception);
}

// KjAsyncState constructor - uses kj::setupAsyncIo() for real async I/O
EventLoop::KjAsyncState::KjAsyncState(EventStats& stats)
    : io_context(kj::setupAsyncIo()), error_handler(kj::heap<TaskSetErrorHandler>(stats)),
      task_set(kj::heap<kj::TaskSet>(*error_handler)) {}

EventLoop::EventLoop() = default;

EventLoop::~EventLoop() {
  stop();
}

void EventLoop::post(kj::Function<void()> task) {
  post(kj::mv(task), EventPriority::Normal);
}

void EventLoop::post_delayed(kj::Function<void()> task, std::chrono::milliseconds delay) {
  post_delayed(kj::mv(task), delay, EventPriority::Normal);
}

void EventLoop::post(kj::Function<void()> task, EventPriority priority) {
  post_with_tags(kj::mv(task), priority, {});
}

void EventLoop::post_with_tags(kj::Function<void()> task, kj::Vector<EventTag> tags) {
  post_with_tags(kj::mv(task), EventPriority::Normal, kj::mv(tags));
}

void EventLoop::post_with_tags(kj::Function<void()> task, EventPriority priority,
                               kj::Vector<EventTag> tags) {
  Task t;
  t.task = kj::mv(task);
  t.priority = priority;
  t.tags = kj::mv(tags);
  t.enqueue_time = std::chrono::steady_clock::now();
  {
    auto lock = queue_state_.lockExclusive();
    lock->tasks.push(kj::mv(t));
    stats_.total_events++;
    stats_.events_by_priority[static_cast<uint8_t>(priority)]++;
  }

  // Wake up the event loop if it's waiting
  {
    auto lock = wake_fulfiller_.lockExclusive();
    KJ_IF_SOME(fulfiller, *lock) {
      fulfiller->fulfill();
      *lock = kj::none;
    }
  }
}

void EventLoop::post_delayed(kj::Function<void()> task, std::chrono::milliseconds delay,
                             EventPriority priority) {
  post_delayed(kj::mv(task), delay, priority, {});
}

void EventLoop::post_delayed(kj::Function<void()> task, std::chrono::milliseconds delay,
                             kj::Vector<EventTag> tags) {
  post_delayed(kj::mv(task), delay, EventPriority::Normal, kj::mv(tags));
}

void EventLoop::post_delayed(kj::Function<void()> task, std::chrono::milliseconds delay,
                             EventPriority priority, kj::Vector<EventTag> tags) {
  auto deadline = std::chrono::steady_clock::now() + delay;
  Task t;
  t.task = kj::mv(task);
  t.priority = priority;
  t.tags = kj::mv(tags);
  t.enqueue_time = std::chrono::steady_clock::now();
  {
    auto lock = queue_state_.lockExclusive();
    lock->delayed_tasks.push({deadline, kj::mv(t)});
    stats_.total_delayed_events++;
  }

  // Wake up the event loop if it's waiting
  {
    auto lock = wake_fulfiller_.lockExclusive();
    KJ_IF_SOME(fulfiller, *lock) {
      fulfiller->fulfill();
      *lock = kj::none;
    }
  }
}

bool EventLoop::should_process_task(const Task& task) {
  auto tags = task.tags.asPtr();

  // Check priority-based filters
  auto lock = filter_state_.lockExclusive();
  for (auto& entry : lock->filters) {
    uint64_t id = entry.key;
    if (lock->active_filters.find(id) == kj::none) {
      continue;
    }
    auto& filter_entry = entry.value;
    KJ_IF_SOME(pf, filter_entry.priority) {
      if (pf != task.priority) {
        continue;
      }
    }
    if (filter_entry.filter(tags)) {
      stats_.events_filtered++;
      return false;
    }
  }

  // Check tag pattern filters
  for (auto& entry : lock->tag_filters) {
    uint64_t id = entry.key;
    const auto& pattern = entry.value;
    if (lock->active_filters.find(id) == kj::none) {
      continue;
    }
    for (const auto& tag : tags) {
      try {
        if (std::regex_search(tag.cStr(), pattern)) {
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

void EventLoop::route_task(Task& task, kj::Function<void()> wrapped) {
  auto lock = filter_state_.lockExclusive();
  KJ_IF_SOME(router, lock->router) {
    router(task.tags.asPtr(), kj::mv(wrapped));
  }
  else {
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
    kj::Function<void()> wrapped = kj::mv(task.task);
    route_task(task, kj::mv(wrapped));
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

kj::Promise<void> EventLoop::process_next_task() {
  // Process delayed tasks that are ready
  {
    auto lock = queue_state_.lockExclusive();
    auto now = std::chrono::steady_clock::now();

    while (!lock->delayed_tasks.empty() && lock->delayed_tasks.top().deadline <= now) {
      DelayedTask delayed_task = lock->delayed_tasks.popValue();
      if (should_process_task(delayed_task.task)) {
        lock->tasks.push(kj::mv(delayed_task.task));
      }
    }
  }

  // Get next task from queue
  kj::Maybe<Task> maybe_task;
  {
    auto lock = queue_state_.lockExclusive();
    if (!lock->tasks.empty()) {
      Task task = lock->tasks.popValue();
      if (should_process_task(task)) {
        maybe_task = kj::mv(task);
      }
    }
  }

  KJ_IF_SOME(task, maybe_task) {
    execute_task(task);
  }

  return kj::READY_NOW;
}

kj::Promise<void> EventLoop::schedule_delayed_tasks() {
  // Check if there are delayed tasks and calculate wait time
  kj::Maybe<std::chrono::steady_clock::time_point> next_deadline;
  {
    auto lock = queue_state_.lockExclusive();
    if (!lock->delayed_tasks.empty()) {
      next_deadline = lock->delayed_tasks.top().deadline;
    }
  }

  KJ_IF_SOME(deadline, next_deadline) {
    auto now = std::chrono::steady_clock::now();
    if (deadline <= now) {
      // Deadline already passed, process immediately
      return kj::READY_NOW;
    }

    auto delay_ms = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
    if (delay_ms > 0 && kj_state_ != nullptr) {
      auto kj_delay = delay_ms * kj::MILLISECONDS;
      return kj_state_->timer().afterDelay(kj_delay);
    }
  }

  return kj::READY_NOW;
}

void EventLoop::run() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    return;
  }

  stop_requested_.store(false);

  // Create KJ async infrastructure on the stack using setupAsyncIo()
  // This provides real OS-level async I/O with proper timer support
  KjAsyncState kj_state(stats_);
  kj::WaitScope& wait_scope = kj_state.io_context.waitScope;

  // Store pointer to state for timer access (cleared before destruction)
  kj_state_ = &kj_state;

  // Main event loop using KJ async primitives
  // Uses proper promise-based waiting instead of polling
  while (!stop_requested_.load()) {
    // Process all pending tasks
    bool has_tasks = true;
    while (has_tasks && !stop_requested_.load()) {
      {
        auto lock = queue_state_.lockExclusive();
        auto now = std::chrono::steady_clock::now();

        // Move ready delayed tasks to the main queue
        while (!lock->delayed_tasks.empty() && lock->delayed_tasks.top().deadline <= now) {
          DelayedTask delayed_task = lock->delayed_tasks.popValue();
          if (should_process_task(delayed_task.task)) {
            lock->tasks.push(kj::mv(delayed_task.task));
          }
        }

        has_tasks = !lock->tasks.empty();
      }

      if (has_tasks) {
        kj::Maybe<Task> maybe_task;
        {
          auto lock = queue_state_.lockExclusive();
          if (!lock->tasks.empty()) {
            Task task = lock->tasks.popValue();
            if (should_process_task(task)) {
              maybe_task = kj::mv(task);
            }
          }
        }

        KJ_IF_SOME(task, maybe_task) {
          execute_task(task);
        }
      }
    }

    if (stop_requested_.load()) {
      break;
    }

    // Calculate wait time for next delayed task
    kj::Maybe<std::chrono::milliseconds> wait_time;
    {
      auto lock = queue_state_.lockExclusive();
      if (!lock->delayed_tasks.empty()) {
        auto now = std::chrono::steady_clock::now();
        auto deadline = lock->delayed_tasks.top().deadline;
        if (deadline > now) {
          wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        }
      }
    }

    // Wait for new tasks or timeout using KJ promises
    auto paf = kj::newPromiseAndCrossThreadFulfiller<void>();
    {
      auto lock = wake_fulfiller_.lockExclusive();
      *lock = kj::mv(paf.fulfiller);
    }

    // Determine wait duration - use delayed task deadline or default poll interval
    kj::Duration kj_delay;
    KJ_IF_SOME(delay, wait_time) {
      kj_delay = delay.count() * kj::MILLISECONDS;
    }
    else {
      // No delayed tasks - use a longer default interval since we have
      // proper cross-thread wake-up via the fulfiller
      kj_delay = 100 * kj::MILLISECONDS;
    }

    // Create timeout promise using KJ timer from setupAsyncIo()
    // This is a real OS-level timer that works without manual advancement
    auto timeout_promise = kj_state_->timer().afterDelay(kj_delay);
    auto wake_promise = kj::mv(paf.promise);

    // Use exclusiveJoin to wait for either wake-up or timeout
    // This is the KJ-idiomatic way to wait without busy-polling
    auto combined = wake_promise.exclusiveJoin(kj::mv(timeout_promise));

    // Wait on the combined promise using KJ's event loop
    // With setupAsyncIo(), wait() properly blocks on OS-level I/O
    // and wakes up when either the timer fires or the fulfiller is triggered
    if (!stop_requested_.load()) {
      combined.wait(wait_scope);
    }

    // Clear the wake fulfiller
    {
      auto lock = wake_fulfiller_.lockExclusive();
      *lock = kj::none;
    }
  }

  // Clear state pointer before destruction
  kj_state_ = nullptr;
  running_.store(false);
  // WaitScope and KjAsyncState will be destroyed in correct order when function returns
}

void EventLoop::stop() {
  stop_requested_.store(true);

  // Wake up the event loop if it's waiting
  {
    auto lock = wake_fulfiller_.lockExclusive();
    KJ_IF_SOME(fulfiller, *lock) {
      fulfiller->fulfill();
      *lock = kj::none;
    }
  }
}

bool EventLoop::is_running() const {
  return running_.load();
}

size_t EventLoop::pending_tasks() const {
  auto lock = queue_state_.lockExclusive();
  return lock->tasks.size() + lock->delayed_tasks.size();
}

size_t EventLoop::pending_tasks_by_priority(EventPriority priority) const {
  auto lock = queue_state_.lockExclusive();
  size_t count = 0;
  // Note: We can't efficiently count by priority in a priority queue without iterating
  // This is O(n) but acceptable for status queries
  // Use underlying data() to iterate without copying (PriorityQueue is move-only)
  const auto& tasks_data = lock->tasks.data();
  for (const auto& task : tasks_data) {
    if (task.priority == priority) {
      count++;
    }
  }
  return count;
}

kj::String EventLoop::stats_to_string() const {
  std::ostringstream oss;
  oss << std::fixed;
  oss << "(\nEventLoop Statistics:\n";
  oss << "  Total Events: " << stats_.total_events.load() << "\n";
  oss << "  Delayed Events: " << stats_.total_delayed_events.load() << "\n";
  oss << "  Events Processed: " << stats_.events_processed.load() << "\n";
  oss << "  Events Failed: " << stats_.events_failed.load() << "\n";
  oss << "  Events Filtered: " << stats_.events_filtered.load() << "\n";
  oss << "  By Priority:\n";
  oss << "    Low: " << stats_.events_by_priority[0].load() << "\n";
  oss << "    Normal: " << stats_.events_by_priority[1].load() << "\n";
  oss << "    High: " << stats_.events_by_priority[2].load() << "\n";
  oss << "    Critical: " << stats_.events_by_priority[3].load() << "\n";
  oss << std::setprecision(3);
  oss << "  Processing Time: " << stats_.processing_time_ns.load() / 1e6 << " ms\n";
  oss << "  Max Processing Time: " << stats_.max_processing_time_ns.load() / 1e6 << " ms\n";
  double avg_wait = stats_.events_processed.load() > 0
                        ? (stats_.queue_wait_time_ns.load() / 1e6) / stats_.events_processed.load()
                        : 0.0;
  oss << "  Avg Queue Wait Time: " << avg_wait << " ms\n";
  oss << "  Max Queue Wait Time: " << stats_.max_queue_wait_time_ns.load() / 1e6 << " ms\n)";
  return kj::str(oss.str().c_str());
}

uint64_t EventLoop::add_filter(EventFilter filter, kj::Maybe<EventPriority> priority) {
  auto lock = filter_state_.lockExclusive();
  uint64_t id = lock->next_filter_id++;
  lock->filters.insert(id, FilterState::FilterEntry{kj::mv(filter), priority});
  lock->active_filters.insert(id);
  return id;
}

void EventLoop::remove_filter(uint64_t filter_id) {
  auto lock = filter_state_.lockExclusive();
  KJ_IF_SOME(entry, lock->active_filters.find(filter_id)) {
    lock->active_filters.erase(entry);
  }
  lock->filters.erase(filter_id);
}

void EventLoop::clear_filters() {
  auto lock = filter_state_.lockExclusive();
  // KJ containers don't have clear(), recreate empty containers
  lock->active_filters = kj::HashSet<uint64_t>();
  lock->filters = kj::HashMap<uint64_t, FilterState::FilterEntry>();
  lock->tag_filters = kj::HashMap<uint64_t, std::regex>();
}

uint64_t EventLoop::add_tag_filter(kj::StringPtr tag_pattern) {
  auto lock = filter_state_.lockExclusive();
  uint64_t id = lock->next_filter_id++;
  try {
    lock->tag_filters.insert(id, std::regex(tag_pattern.cStr()));
    lock->active_filters.insert(id);
  } catch (const std::regex_error& e) {
    // Remove ID if regex creation failed
    KJ_IF_SOME(entry, lock->active_filters.find(id)) {
      lock->active_filters.erase(entry);
    }
    KJ_FAIL_REQUIRE("Invalid tag pattern", tag_pattern.cStr(), e.what());
  }
  return id;
}

void EventLoop::remove_tag_filter(uint64_t filter_id) {
  auto lock = filter_state_.lockExclusive();
  KJ_IF_SOME(entry, lock->active_filters.find(filter_id)) {
    lock->active_filters.erase(entry);
  }
  lock->tag_filters.erase(filter_id);
}

void EventLoop::set_router(EventRouter router) {
  auto lock = filter_state_.lockExclusive();
  lock->router = kj::mv(router);
}

void EventLoop::clear_router() {
  auto lock = filter_state_.lockExclusive();
  lock->router = kj::none;
}

} // namespace veloz::core
