#pragma once

#include "veloz/core/event_loop.h"
#include "veloz/core/lockfree_queue.h"
#include "veloz/core/timer_wheel.h"

#include <atomic>
#include <chrono>
#include <kj/async.h>
#include <kj/common.h>
#include <kj/mutex.h>
#include <kj/timer.h>

namespace veloz::core {

/**
 * @brief Optimized EventLoop with lock-free task queue and hierarchical timer wheel
 *
 * This implementation provides better performance than the standard EventLoop by:
 * 1. Using a lock-free MPMC queue for task submission (no lock contention)
 * 2. Using a hierarchical timer wheel for efficient delayed task scheduling (O(1))
 * 3. Reducing memory allocation overhead through node pooling
 *
 * The architecture is:
 * - Multiple producers submit tasks to the lock-free queue
 * - Single consumer (event loop thread) drains the queue and processes tasks
 * - Timer wheel handles delayed tasks with O(1) insertion and firing
 */
class OptimizedEventLoop final {
public:
  OptimizedEventLoop();
  ~OptimizedEventLoop();

  OptimizedEventLoop(const OptimizedEventLoop&) = delete;
  OptimizedEventLoop& operator=(const OptimizedEventLoop&) = delete;

  // Basic task posting (lock-free, can be called from any thread)
  void post(std::function<void()> task);
  void post(std::function<void()> task, EventPriority priority);
  void post_delayed(std::function<void()> task, std::chrono::milliseconds delay);
  void post_delayed(std::function<void()> task, std::chrono::milliseconds delay,
                    EventPriority priority);

  // Event loop control
  void run();
  void stop();

  // Status queries
  [[nodiscard]] bool is_running() const;
  [[nodiscard]] size_t pending_tasks() const;

  // Statistics
  [[nodiscard]] const EventStats& stats() const {
    return stats_;
  }
  void reset_stats() {
    stats_.reset();
  }

  // Performance metrics specific to optimized implementation
  struct OptimizedStats {
    std::atomic<uint64_t> lockfree_queue_pushes{0};
    std::atomic<uint64_t> lockfree_queue_pops{0};
    std::atomic<uint64_t> timer_wheel_schedules{0};
    std::atomic<uint64_t> timer_wheel_fires{0};
    std::atomic<uint64_t> batch_sizes{0};
    std::atomic<uint64_t> batch_count{0};

    void reset() {
      lockfree_queue_pushes.store(0);
      lockfree_queue_pops.store(0);
      timer_wheel_schedules.store(0);
      timer_wheel_fires.store(0);
      batch_sizes.store(0);
      batch_count.store(0);
    }
  };

  [[nodiscard]] const OptimizedStats& optimized_stats() const {
    return opt_stats_;
  }
  void reset_optimized_stats() {
    opt_stats_.reset();
  }

private:
  // Task wrapper for the lock-free queue
  struct QueuedTask {
    std::function<void()> task;
    EventPriority priority;
    std::chrono::steady_clock::time_point enqueue_time;
  };

  // Execute a single task with timing
  void execute_task(QueuedTask& task);

  // Process tasks from the lock-free queue
  size_t drain_queue();

  // Advance timer wheel and fire expired timers
  size_t process_timers();

  // Lock-free queue for immediate tasks
  LockFreeQueue<QueuedTask> task_queue_;

  // Timer wheel for delayed tasks
  HierarchicalTimerWheel timer_wheel_;

  // Atomic counters for pending tasks
  std::atomic<size_t> pending_immediate_{0};
  std::atomic<size_t> pending_delayed_{0};

  // Event loop state
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};

  // Wake-up mechanism using KJ cross-thread fulfiller
  kj::MutexGuarded<kj::Maybe<kj::Own<kj::CrossThreadPromiseFulfiller<void>>>> wake_fulfiller_;

  // Statistics
  EventStats stats_;
  OptimizedStats opt_stats_;

  // Last timer wheel tick time
  std::chrono::steady_clock::time_point last_tick_time_;
};

} // namespace veloz::core
