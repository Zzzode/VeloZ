#pragma once

#include <atomic>
#include <chrono>  // std::chrono::steady_clock - KJ time types don't provide steady_clock equivalent
#include <cstdint>
#include <functional>  // std::function - kj::Function is not copyable, std::priority_queue requires copyable elements
#include <kj/async.h>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/memory.h>  // kj::Maybe used for priority filter
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/table.h>
#include <kj/timer.h>
#include <kj/vector.h>
#include <queue>   // std::priority_queue - KJ does not provide a priority queue implementation
#include <regex>   // std::regex - KJ does not provide regex functionality
#include <vector>  // std::vector - used with std::priority_queue which requires STL containers

namespace veloz::core {

// =======================================================================================
// std Library Usage Justification (KJ Migration Analysis)
// =======================================================================================
//
// The following std library types are retained because KJ does not provide equivalents
// or the KJ equivalents have limitations that prevent their use in this context:
//
// 1. std::function<void()>
//    - Used for: Task callbacks, EventFilter, EventRouter
//    - Why not kj::Function: kj::Function is not copyable, but std::priority_queue
//      requires copyable elements. Task structs containing callbacks must be copyable
//      for the priority queue to work correctly.
//
// 2. std::priority_queue<Task, std::vector<Task>, std::greater<>>
//    - Used for: Priority-based task scheduling
//    - Why not KJ: KJ does not provide a priority queue implementation. The priority
//      queue is essential for executing higher-priority events before lower-priority ones.
//
// 3. std::vector<EventTag>
//    - Used for: Storing event tags in Task struct
//    - Why not kj::Vector: std::priority_queue requires STL-compatible containers.
//      Using kj::Vector would require custom adapters for std::priority_queue.
//
// 4. std::regex
//    - Used for: Tag pattern filtering (add_tag_filter)
//    - Why not KJ: KJ does not provide regex functionality. Tag filtering requires
//      pattern matching capabilities that only std::regex provides.
//
// 5. std::chrono::steady_clock
//    - Used for: Task enqueue timestamps, deadline tracking
//    - Why not KJ: kj::TimePoint and kj::Duration are designed for async I/O timing,
//      not for measuring elapsed wall-clock time. std::chrono::steady_clock provides
//      monotonic time suitable for measuring task queue wait times and deadlines.
//
// =======================================================================================

/**
 * @brief Event priority enumeration
 *
 * Defines the priority levels for events in the event loop.
 * Higher priority events are executed before lower priority ones.
 */
enum class EventPriority : uint8_t {
  Low = 0,     ///< Low priority events (background tasks, cleanup)
  Normal = 1,  ///< Normal priority events (default)
  High = 2,    ///< High priority events (important but not critical)
  Critical = 3 ///< Critical priority events (must execute immediately)
};

/**
 * @brief Convert EventPriority to string
 */
[[nodiscard]] kj::StringPtr to_string(EventPriority priority);

/**
 * @brief Event tag for filtering and routing
 *
 * Events can be tagged with strings to enable filtering and routing
 * based on event types, categories, or sources.
 *
 * @note Uses kj::String for the tag value itself, but stored in std::vector
 *       for STL container compatibility with std::priority_queue.
 */
using EventTag = kj::String;

/**
 * @brief Event filter predicate
 *
 * A filter function that returns true if an event should be processed.
 * The filter receives the event tags and can decide based on them.
 *
 * @note Uses std::function instead of kj::Function because kj::Function is not
 *       copyable, but std::priority_queue requires copyable elements in Task struct.
 * @note Uses std::vector<EventTag> for STL container compatibility with priority_queue.
 */
using EventFilter = std::function<bool(const std::vector<EventTag>&)>;

/**
 * @brief Event routing function
 *
 * Routes an event to a specific handler based on its tags.
 *
 * @note Uses std::function instead of kj::Function because kj::Function is not
 *       copyable, but std::priority_queue requires copyable elements in Task struct.
 * @note Uses std::vector<EventTag> for STL container compatibility with priority_queue.
 */
using EventRouter = std::function<void(const std::vector<EventTag>&, std::function<void()>)>;

/**
 * @brief Event statistics
 *
 * Contains metrics about event processing.
 */
struct EventStats {
  std::atomic<uint64_t> total_events{0};
  std::atomic<uint64_t> total_delayed_events{0};
  std::atomic<uint64_t> events_processed{0};
  std::atomic<uint64_t> events_failed{0};
  std::atomic<uint64_t> events_filtered{0};
  std::atomic<uint64_t> events_by_priority[4]{{0}, {0}, {0}, {0}};
  std::atomic<uint64_t> processing_time_ns{0};
  std::atomic<uint64_t> max_processing_time_ns{0};
  std::atomic<uint64_t> queue_wait_time_ns{0};
  std::atomic<uint64_t> max_queue_wait_time_ns{0};

  void reset() {
    total_events.store(0);
    total_delayed_events.store(0);
    events_processed.store(0);
    events_failed.store(0);
    events_filtered.store(0);
    for (auto& count : events_by_priority) {
      count.store(0);
    }
    processing_time_ns.store(0);
    max_processing_time_ns.store(0);
    queue_wait_time_ns.store(0);
    max_queue_wait_time_ns.store(0);
  }
};

class EventLoop final {
public:
  EventLoop();
  ~EventLoop();

  EventLoop(const EventLoop&) = delete;
  EventLoop& operator=(const EventLoop&) = delete;

  // Basic task posting
  void post(std::function<void()> task);
  void post_delayed(std::function<void()> task, std::chrono::milliseconds delay);

  // Priority-based task posting
  void post(std::function<void()> task, EventPriority priority);
  void post_with_tags(std::function<void()> task, std::vector<EventTag> tags);
  void post_with_tags(std::function<void()> task, EventPriority priority,
                      std::vector<EventTag> tags);
  void post_delayed(std::function<void()> task, std::chrono::milliseconds delay,
                    EventPriority priority);
  void post_delayed(std::function<void()> task, std::chrono::milliseconds delay,
                    std::vector<EventTag> tags);
  void post_delayed(std::function<void()> task, std::chrono::milliseconds delay,
                    EventPriority priority, std::vector<EventTag> tags);

  // Event loop control
  void run();
  void stop();

  // Status queries
  [[nodiscard]] bool is_running() const;
  [[nodiscard]] size_t pending_tasks() const;
  [[nodiscard]] size_t pending_tasks_by_priority(EventPriority priority) const;

  // Statistics
  [[nodiscard]] const EventStats& stats() const {
    return stats_;
  }
  void reset_stats() {
    stats_.reset();
  }
  [[nodiscard]] kj::String stats_to_string() const;

  // Filtering
  /**
   * @brief Add a filter that excludes events matching the predicate
   * @param filter Function that returns true if event should be excluded
   * @param priority Optional priority level to filter (if empty, applies to all)
   * @return Filter ID for removal
   */
  [[nodiscard]] uint64_t add_filter(EventFilter filter,
                                    kj::Maybe<EventPriority> priority = kj::none);
  void remove_filter(uint64_t filter_id);
  void clear_filters();

  // Tag-based filtering
  /**
   * @brief Add a tag pattern filter
   * Events with tags matching the regex pattern will be filtered out
   * @param tag_pattern Regex pattern for tag matching
   * @return Filter ID for removal
   */
  [[nodiscard]] uint64_t add_tag_filter(kj::StringPtr tag_pattern);
  void remove_tag_filter(uint64_t filter_id);

  // Event routing
  /**
   * @brief Set a custom event router
   * The router can redirect events to different handlers based on tags
   */
  void set_router(EventRouter router);
  void clear_router();

private:
  /**
   * @brief Internal task representation for the priority queue
   *
   * @note std library types are used here because:
   *   - std::function<void()>: kj::Function is not copyable, but std::priority_queue
   *     requires copyable elements. Task must be copyable for queue operations.
   *   - std::vector<EventTag>: Required for STL container compatibility with
   *     std::priority_queue. kj::Vector would require custom adapters.
   *   - std::chrono::steady_clock::time_point: KJ time types (kj::TimePoint) are
   *     designed for async I/O timing, not for measuring elapsed wall-clock time.
   *     steady_clock provides monotonic time for queue wait time measurement.
   */
  struct Task {
    std::function<void()> task;  // std::function required: kj::Function not copyable
    EventPriority priority;
    std::vector<EventTag> tags;  // std::vector required: STL container for priority_queue
    std::chrono::steady_clock::time_point enqueue_time;  // steady_clock: monotonic time

    Task() = default;
    Task(const Task& other)
        : task(other.task), priority(other.priority), tags(), enqueue_time(other.enqueue_time) {
      tags.reserve(other.tags.size());
      for (const auto& tag : other.tags) {
        tags.emplace_back(kj::heapString(tag));
      }
    }
    Task& operator=(const Task& other) {
      if (this == &other) {
        return *this;
      }
      task = other.task;
      priority = other.priority;
      enqueue_time = other.enqueue_time;
      tags.clear();
      tags.reserve(other.tags.size());
      for (const auto& tag : other.tags) {
        tags.emplace_back(kj::heapString(tag));
      }
      return *this;
    }

    bool operator>(const Task& other) const {
      if (priority != other.priority) {
        return static_cast<uint8_t>(priority) < static_cast<uint8_t>(other.priority);
      }
      return enqueue_time > other.enqueue_time;
    }
  };

  struct DelayedTask {
    std::chrono::steady_clock::time_point deadline;
    Task task;
    bool operator>(const DelayedTask& other) const {
      return deadline > other.deadline;
    }
  };

  // Task execution
  void execute_task(Task& task);
  bool should_process_task(const Task& task);
  void route_task(Task& task, std::function<void()> wrapped);

  // KJ TaskSet error handler for async task failures
  class TaskSetErrorHandler : public kj::TaskSet::ErrorHandler {
  public:
    explicit TaskSetErrorHandler(EventStats& stats) : stats_(stats) {}
    void taskFailed(kj::Exception&& exception) override;

  private:
    EventStats& stats_;
  };

  // Internal method to process tasks from the queue using KJ promises
  kj::Promise<void> process_next_task();
  kj::Promise<void> schedule_delayed_tasks();

  /**
   * @brief Queue state (protected by KJ mutex for thread-safe access)
   *
   * @note Uses std::priority_queue because KJ does not provide a priority queue
   *       implementation. Priority-based task scheduling is essential for executing
   *       higher-priority events (e.g., Critical) before lower-priority ones (e.g., Low).
   * @note Uses std::vector as the underlying container because std::priority_queue
   *       requires STL-compatible containers.
   */
  struct QueueState {
    // std::priority_queue required: KJ does not provide priority queue implementation
    std::priority_queue<Task, std::vector<Task>, std::greater<>> tasks;
    std::priority_queue<DelayedTask, std::vector<DelayedTask>, std::greater<>> delayed_tasks;
  };

  /**
   * @brief Filter state (protected by KJ mutex)
   *
   * @note Uses std::regex for tag pattern matching because KJ does not provide
   *       regex functionality. Tag filtering requires pattern matching capabilities.
   * @note Uses kj::HashMap and kj::HashSet for filter storage (KJ equivalents available).
   */
  struct FilterState {
    kj::HashSet<uint64_t> active_filters;
    uint64_t next_filter_id = 0;
    kj::HashMap<uint64_t, std::pair<EventFilter, kj::Maybe<EventPriority>>> filters;
    kj::HashMap<uint64_t, std::regex> tag_filters;  // std::regex required: KJ has no regex
    kj::Maybe<EventRouter> router;
  };

  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};

  // KJ async primitives - using MutexGuarded for thread-safe queue access
  kj::MutexGuarded<QueueState> queue_state_;
  kj::MutexGuarded<FilterState> filter_state_;

  // KJ event loop infrastructure (created on stack in run(), pointer stored for timer access)
  struct KjAsyncState {
    kj::EventLoop event_loop;
    kj::Own<TaskSetErrorHandler> error_handler;
    kj::Own<kj::TaskSet> task_set;
    kj::Own<kj::TimerImpl> timer;

    explicit KjAsyncState(EventStats& stats);
  };
  KjAsyncState* kj_state_ = nullptr; // Non-owning pointer to stack-allocated state

  // Cross-thread wake-up using KJ's cross-thread fulfiller
  kj::MutexGuarded<kj::Maybe<kj::Own<kj::CrossThreadPromiseFulfiller<void>>>> wake_fulfiller_;

  // Statistics
  EventStats stats_;
};

} // namespace veloz::core
