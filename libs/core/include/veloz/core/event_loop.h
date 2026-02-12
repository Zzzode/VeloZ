#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <chrono>
#include <optional>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <regex>

namespace veloz::core {

/**
 * @brief Event priority enumeration
 *
 * Defines the priority levels for events in the event loop.
 * Higher priority events are executed before lower priority ones.
 */
enum class EventPriority : uint8_t {
  Low = 0,       ///< Low priority events (background tasks, cleanup)
  Normal = 1,    ///< Normal priority events (default)
  High = 2,      ///< High priority events (important but not critical)
  Critical = 3   ///< Critical priority events (must execute immediately)
};

/**
 * @brief Convert EventPriority to string
 */
[[nodiscard]] std::string_view to_string(EventPriority priority);

/**
 * @brief Event tag for filtering and routing
 *
 * Events can be tagged with strings to enable filtering and routing
 * based on event types, categories, or sources.
 */
using EventTag = std::string;

/**
 * @brief Event filter predicate
 *
 * A filter function that returns true if an event should be processed.
 * The filter receives the event tags and can decide based on them.
 */
using EventFilter = std::function<bool(const std::vector<EventTag>&)>;

/**
 * @brief Event routing function
 *
 * Routes an event to a specific handler based on its tags.
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
  void post_with_tags(std::function<void()> task, EventPriority priority, std::vector<EventTag> tags);
  void post_delayed(std::function<void()> task, std::chrono::milliseconds delay, EventPriority priority);
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
  [[nodiscard]] const EventStats& stats() const { return stats_; }
  void reset_stats() { stats_.reset(); }
  [[nodiscard]] std::string stats_to_string() const;

  // Filtering
  /**
   * @brief Add a filter that excludes events matching the predicate
   * @param filter Function that returns true if event should be excluded
   * @param priority Optional priority level to filter (if empty, applies to all)
   * @return Filter ID for removal
   */
  [[nodiscard]] uint64_t add_filter(EventFilter filter, std::optional<EventPriority> priority = std::nullopt);
  void remove_filter(uint64_t filter_id);
  void clear_filters();

  // Tag-based filtering
  /**
   * @brief Add a tag pattern filter
   * Events with tags matching the regex pattern will be filtered out
   * @param tag_pattern Regex pattern for tag matching
   * @return Filter ID for removal
   */
  [[nodiscard]] uint64_t add_tag_filter(const std::string& tag_pattern);
  void remove_tag_filter(uint64_t filter_id);

  // Event routing
  /**
   * @brief Set a custom event router
   * The router can redirect events to different handlers based on tags
   */
  void set_router(EventRouter router);
  void clear_router();

private:
  struct Task {
    std::function<void()> task;
    EventPriority priority;
    std::vector<EventTag> tags;
    std::chrono::steady_clock::time_point enqueue_time;

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

  // Filtering
  std::unordered_set<uint64_t> active_filters_;
  uint64_t next_filter_id_{1};

  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};

  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::priority_queue<Task, std::vector<Task>, std::greater<>> tasks_;
  std::priority_queue<DelayedTask, std::vector<DelayedTask>, std::greater<>> delayed_tasks_;

  // Filtering and routing
  std::unordered_map<uint64_t, std::pair<EventFilter, std::optional<EventPriority>>> filters_;
  std::unordered_map<uint64_t, std::regex> tag_filters_;
  std::optional<EventRouter> router_;

  // Statistics
  EventStats stats_;
};

} // namespace veloz::core
