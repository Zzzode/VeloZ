#include "veloz/core/optimized_event_loop.h"

#include <kj/async.h>
#include <kj/debug.h>
#include <kj/time.h>
#include <thread>

namespace veloz::core {

OptimizedEventLoop::OptimizedEventLoop() : last_tick_time_(std::chrono::steady_clock::now()) {}

OptimizedEventLoop::~OptimizedEventLoop() {
  stop();
}

void OptimizedEventLoop::post(std::function<void()> task) {
  post(kj::mv(task), EventPriority::Normal);
}

void OptimizedEventLoop::post(std::function<void()> task, EventPriority priority) {
  QueuedTask qt{
      .task = kj::mv(task), .priority = priority, .enqueue_time = std::chrono::steady_clock::now()};

  task_queue_.push(kj::mv(qt));
  pending_immediate_.fetch_add(1, std::memory_order_relaxed);

  stats_.total_events++;
  stats_.events_by_priority[static_cast<uint8_t>(priority)]++;
  opt_stats_.lockfree_queue_pushes++;

  // Wake up the event loop if it's waiting
  {
    auto lock = wake_fulfiller_.lockExclusive();
    KJ_IF_SOME(fulfiller, *lock) {
      fulfiller->fulfill();
      *lock = kj::none;
    }
  }
}

void OptimizedEventLoop::post_delayed(std::function<void()> task, std::chrono::milliseconds delay) {
  post_delayed(kj::mv(task), delay, EventPriority::Normal);
}

void OptimizedEventLoop::post_delayed(std::function<void()> task, std::chrono::milliseconds delay,
                                      EventPriority priority) {
  // Capture priority and enqueue time for the delayed task
  auto enqueue_time = std::chrono::steady_clock::now();
  auto wrapped_task = [this, task = kj::mv(task), priority, enqueue_time]() mutable {
    QueuedTask qt{.task = kj::mv(task), .priority = priority, .enqueue_time = enqueue_time};
    execute_task(qt);
  };

  timer_wheel_.schedule(static_cast<uint64_t>(delay.count()), kj::mv(wrapped_task));
  pending_delayed_.fetch_add(1, std::memory_order_relaxed);

  stats_.total_delayed_events++;
  opt_stats_.timer_wheel_schedules++;

  // Wake up the event loop if it's waiting
  {
    auto lock = wake_fulfiller_.lockExclusive();
    KJ_IF_SOME(fulfiller, *lock) {
      fulfiller->fulfill();
      *lock = kj::none;
    }
  }
}

void OptimizedEventLoop::execute_task(QueuedTask& task) {
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
    task.task();
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

size_t OptimizedEventLoop::drain_queue() {
  size_t processed = 0;
  constexpr size_t MAX_BATCH_SIZE = 256;

  while (processed < MAX_BATCH_SIZE) {
    auto maybe_task = task_queue_.pop();
    KJ_IF_SOME(task, maybe_task) {
      pending_immediate_.fetch_sub(1, std::memory_order_relaxed);
      opt_stats_.lockfree_queue_pops++;

      execute_task(task);
      processed++;
    } else {
      break;
    }
  }

  if (processed > 0) {
    opt_stats_.batch_sizes += processed;
    opt_stats_.batch_count++;
  }

  return processed;
}

size_t OptimizedEventLoop::process_timers() {
  auto now = std::chrono::steady_clock::now();
  auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick_time_).count();

  if (elapsed_ms <= 0) {
    return 0;
  }

  last_tick_time_ = now;

  // Advance the timer wheel by the elapsed time
  size_t fired = timer_wheel_.advance(static_cast<uint64_t>(elapsed_ms));

  if (fired > 0) {
    pending_delayed_.fetch_sub(fired, std::memory_order_relaxed);
    opt_stats_.timer_wheel_fires += fired;
  }

  return fired;
}

void OptimizedEventLoop::run() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    return;
  }

  stop_requested_.store(false);
  last_tick_time_ = std::chrono::steady_clock::now();

  // Create KJ event loop for cross-thread wake-up
  kj::EventLoop kj_event_loop;
  kj::WaitScope wait_scope(kj_event_loop);

  while (!stop_requested_.load()) {
    // Process timers first (may add tasks to the queue)
    process_timers();

    // Drain the lock-free queue
    size_t processed = drain_queue();

    // If no tasks were processed, wait for new tasks or timer
    if (processed == 0 && !stop_requested_.load()) {
      // Calculate wait time until next timer
      uint64_t next_tick = timer_wheel_.next_timer_tick();
      uint64_t current_tick = timer_wheel_.current_tick();

      std::chrono::milliseconds wait_time(1); // Default 1ms poll interval

      if (next_tick != UINT64_MAX && next_tick > current_tick) {
        uint64_t delay = next_tick - current_tick;
        if (delay < 1000) {
          wait_time = std::chrono::milliseconds(delay);
        }
      }

      // Set up cross-thread wake-up
      auto paf = kj::newPromiseAndCrossThreadFulfiller<void>();
      {
        auto lock = wake_fulfiller_.lockExclusive();
        *lock = kj::mv(paf.fulfiller);
      }

      // Brief sleep to avoid busy-waiting
      // In a production system, we'd use proper OS-level wake-up mechanisms
      std::this_thread::sleep_for(kj::min(wait_time, std::chrono::milliseconds(1)));

      // Clear the wake fulfiller
      {
        auto lock = wake_fulfiller_.lockExclusive();
        *lock = kj::none;
      }
    }
  }

  running_.store(false);
}

void OptimizedEventLoop::stop() {
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

bool OptimizedEventLoop::is_running() const {
  return running_.load();
}

size_t OptimizedEventLoop::pending_tasks() const {
  return pending_immediate_.load(std::memory_order_relaxed) +
         pending_delayed_.load(std::memory_order_relaxed);
}

} // namespace veloz::core
