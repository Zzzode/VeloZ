#pragma once

#include "veloz/core/priority_queue.h" // KJ-native priority queue implementation

#include <atomic>
#include <chrono> // std::chrono::steady_clock - KJ time types don't provide steady_clock equivalent
#include <cstdint>
#include <kj/async-io.h>
#include <kj/async.h>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/memory.h> // kj::Maybe used for priority filter
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/table.h>
#include <kj/timer.h>
#include <kj/vector.h>
#include <regex> // std::regex - KJ does not provide regex functionality

namespace veloz::core {

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
 * @note EventTag is a kj::String. The std::priority_queue compatibility note applies to the
 *       Task container in this file, not to EventTag itself.
 */
using EventTag = kj::String;

/**
 * @brief Event filter predicate
 *
 * A filter function that returns true if an event should be processed.
 * The filter receives the event tags and can decide based on them.
 */
using EventFilter = kj::Function<bool(kj::ArrayPtr<const EventTag>)>;

/**
 * @brief Event routing function
 *
 * Routes an event to a specific handler based on its tags.
 */
using EventRouter = kj::Function<void(kj::ArrayPtr<const EventTag>, kj::Function<void()>)>;

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
  void post(kj::Function<void()> task);
  void post_delayed(kj::Function<void()> task, std::chrono::milliseconds delay);

  // Priority-based task posting
  void post(kj::Function<void()> task, EventPriority priority);
  void post_with_tags(kj::Function<void()> task, kj::Vector<EventTag> tags);
  void post_with_tags(kj::Function<void()> task, EventPriority priority, kj::Vector<EventTag> tags);
  void post_delayed(kj::Function<void()> task, std::chrono::milliseconds delay,
                    EventPriority priority);
  void post_delayed(kj::Function<void()> task, std::chrono::milliseconds delay,
                    kj::Vector<EventTag> tags);
  void post_delayed(kj::Function<void()> task, std::chrono::milliseconds delay,
                    EventPriority priority, kj::Vector<EventTag> tags);

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
   */
  struct Task {
    kj::Function<void()> task;
    EventPriority priority = EventPriority::Normal;
    kj::Vector<EventTag> tags;
    std::chrono::steady_clock::time_point enqueue_time; // steady_clock: monotonic time

    bool operator>(const Task& other) const {
      if (priority != other.priority) {
        return static_cast<uint8_t>(priority) > static_cast<uint8_t>(other.priority);
      }
      return enqueue_time < other.enqueue_time;
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
  void route_task(Task& task, kj::Function<void()> wrapped);

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
   * @note Uses veloz::core::PriorityQueue (KJ-native implementation) for priority-based
   *       task scheduling. Priority queues are essential for executing higher-priority events
   *       (e.g., Critical) before lower-priority ones (e.g., Low).
   */
  struct QueueState {
    PriorityQueue<Task> tasks;
    PriorityQueue<DelayedTask> delayed_tasks;
  };

  /**
   * @brief Filter state (protected by KJ mutex)
   *
   * @note Uses std::regex for tag pattern matching because KJ does not provide
   *       regex functionality. Tag filtering requires pattern matching capabilities.
   * @note Uses kj::HashMap and kj::HashSet for filter storage (KJ equivalents available).
   */
  struct FilterState {
    struct FilterEntry {
      EventFilter filter;
      kj::Maybe<EventPriority> priority;
    };

    kj::HashSet<uint64_t> active_filters;
    uint64_t next_filter_id = 0;
    kj::HashMap<uint64_t, FilterEntry> filters;
    kj::HashMap<uint64_t, std::regex> tag_filters; // std::regex required: KJ has no regex
    kj::Maybe<EventRouter> router;
  };

  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};

  // KJ async primitives - using MutexGuarded for thread-safe queue access
  kj::MutexGuarded<QueueState> queue_state_;
  kj::MutexGuarded<FilterState> filter_state_;

  // KJ event loop infrastructure (created on stack in run(), pointer stored for timer access)
  // Uses kj::setupAsyncIo() for real async I/O with OS-level timer support
  struct KjAsyncState {
    kj::AsyncIoContext io_context;
    kj::Own<TaskSetErrorHandler> error_handler;
    kj::Own<kj::TaskSet> task_set;

    explicit KjAsyncState(EventStats& stats);

    // Access timer from io_context
    kj::Timer& timer() {
      return io_context.provider->getTimer();
    }
  };
  KjAsyncState* kj_state_ = nullptr; // Non-owning pointer to stack-allocated state

  // Cross-thread wake-up using KJ's cross-thread fulfiller
  kj::MutexGuarded<kj::Maybe<kj::Own<kj::CrossThreadPromiseFulfiller<void>>>> wake_fulfiller_;

  // Statistics
  EventStats stats_;
};

} // namespace veloz::core
