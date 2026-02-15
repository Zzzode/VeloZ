#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/function.h>
#include <kj/memory.h>
#include <kj/time.h>
#include <kj/vector.h>

namespace veloz::core {

/**
 * @brief Timer callback function type
 */
using TimerCallback = kj::Function<void()>;

/**
 * @brief Timer entry in the wheel
 *
 * Linked list entry for timer wheel implementation.
 * Uses raw pointers for O(1) linked list navigation.
 */
struct TimerEntry {
  uint64_t id;
  uint64_t expiration_tick;
  TimerCallback callback;
  TimerEntry* next{nullptr};  // Next entry in list (non-owning)
  TimerEntry* prev{nullptr};  // Previous entry in list (non-owning)

  TimerEntry(uint64_t id_, uint64_t exp_tick, TimerCallback&& cb)
      : id(id_), expiration_tick(exp_tick), callback(kj::mv(cb)) {}
};

/**
 * @brief Hierarchical Timer Wheel for efficient timer management
 *
 * Implements a 4-level hierarchical timer wheel with:
 * - Level 0: 1ms resolution, 256ms range (256 slots)
 * - Level 1: 256ms resolution, ~65s range (256 slots)
 * - Level 2: ~65s resolution, ~4.6 hour range (256 slots)
 * - Level 3: ~4.6 hour resolution, ~49 day range (256 slots)
 *
 * Provides O(1) insertion and O(1) per-timer firing.
 * Thread-safe for single-writer, single-reader (typical event loop pattern).
 */
class HierarchicalTimerWheel {
public:
  static constexpr size_t SLOTS_PER_LEVEL = 256;
  static constexpr size_t NUM_LEVELS = 4;
  static constexpr size_t BITS_PER_LEVEL = 8; // log2(256)

  // Resolution at each level (in ticks, where 1 tick = 1ms)
  static constexpr uint64_t LEVEL_RESOLUTION[NUM_LEVELS] = {
      1,       // Level 0: 1ms
      256,     // Level 1: 256ms
      65536,   // Level 2: ~65s
      16777216 // Level 3: ~4.6 hours
  };

  // Maximum range at each level
  static constexpr uint64_t LEVEL_RANGE[NUM_LEVELS] = {
      256,       // Level 0: 256ms
      65536,     // Level 1: ~65s
      16777216,  // Level 2: ~4.6 hours
      4294967296 // Level 3: ~49 days
  };

  HierarchicalTimerWheel() : current_tick_(0), next_timer_id_(1) {
    // Initialize all slots to nullptr
    for (size_t level = 0; level < NUM_LEVELS; ++level) {
      for (size_t slot = 0; slot < SLOTS_PER_LEVEL; ++slot) {
        wheels_[level][slot] = nullptr;
      }
    }
  }

  ~HierarchicalTimerWheel() {
    // Clean up all remaining timers
    for (size_t level = 0; level < NUM_LEVELS; ++level) {
      for (size_t slot = 0; slot < SLOTS_PER_LEVEL; ++slot) {
        TimerEntry* entry = wheels_[level][slot];
        while (entry != nullptr) {
          TimerEntry* next = entry->next;
          delete entry;
          entry = next;
        }
      }
    }
  }

  // Non-copyable, non-movable
  HierarchicalTimerWheel(const HierarchicalTimerWheel&) = delete;
  HierarchicalTimerWheel& operator=(const HierarchicalTimerWheel&) = delete;
  HierarchicalTimerWheel(HierarchicalTimerWheel&&) = delete;
  HierarchicalTimerWheel& operator=(HierarchicalTimerWheel&&) = delete;

  /**
   * @brief Schedule a timer to fire after the specified delay
   *
   * @param delay_ms Delay in milliseconds
   * @param callback Callback to invoke when timer fires
   * @return Timer ID for cancellation
   */
  uint64_t schedule(uint64_t delay_ms, TimerCallback callback) {
    uint64_t expiration = current_tick_ + delay_ms;
    uint64_t id = next_timer_id_++;

    auto* entry = new TimerEntry(id, expiration, kj::mv(callback));
    insert_timer(entry);

    timer_count_.fetch_add(1, std::memory_order_relaxed);
    return id;
  }

  /**
   * @brief Schedule a timer using KJ Duration
   */
  uint64_t schedule(kj::Duration delay, TimerCallback callback) {
    uint64_t delay_ms = delay / kj::MILLISECONDS;
    return schedule(delay_ms, kj::mv(callback));
  }

  /**
   * @brief Cancel a scheduled timer
   *
   * @param timer_id Timer ID returned by schedule()
   * @return true if timer was found and cancelled
   */
  bool cancel(uint64_t timer_id) {
    // Search all levels for the timer
    for (size_t level = 0; level < NUM_LEVELS; ++level) {
      for (size_t slot = 0; slot < SLOTS_PER_LEVEL; ++slot) {
        TimerEntry* entry = wheels_[level][slot];
        while (entry != nullptr) {
          if (entry->id == timer_id) {
            remove_from_slot(entry, level, slot);
            delete entry;
            timer_count_.fetch_sub(1, std::memory_order_relaxed);
            return true;
          }
          entry = entry->next;
        }
      }
    }
    return false;
  }

  /**
   * @brief Advance the timer wheel by one tick and fire expired timers
   *
   * @return Number of timers fired
   */
  size_t tick() {
    size_t fired = 0;

    // First, cascade from higher levels BEFORE firing level 0
    // This ensures timers that cascade down to level 0 can fire in the same tick
    for (size_t level = 1; level < NUM_LEVELS; ++level) {
      // Check if this level's slot boundary is crossed
      uint64_t level_mask = (1ULL << (BITS_PER_LEVEL * level)) - 1;
      if ((current_tick_ & level_mask) == 0) {
        // Cascade timers from this level down
        size_t slot = (current_tick_ >> (BITS_PER_LEVEL * level)) & (SLOTS_PER_LEVEL - 1);
        cascade_slot(level, slot);
      }
    }

    // Now process level 0 slot for current tick (including any cascaded timers)
    size_t slot0 = current_tick_ & (SLOTS_PER_LEVEL - 1);
    fired += fire_slot(0, slot0);

    current_tick_++;
    return fired;
  }

  /**
   * @brief Advance the timer wheel by multiple ticks
   *
   * @param ticks Number of ticks to advance
   * @return Total number of timers fired
   */
  size_t advance(uint64_t ticks) {
    size_t total_fired = 0;
    for (uint64_t i = 0; i < ticks; ++i) {
      total_fired += tick();
    }
    return total_fired;
  }

  /**
   * @brief Get the current tick count
   */
  [[nodiscard]] uint64_t current_tick() const noexcept {
    return current_tick_;
  }

  /**
   * @brief Get the number of scheduled timers
   */
  [[nodiscard]] size_t timer_count() const noexcept {
    return timer_count_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Check if there are any scheduled timers
   */
  [[nodiscard]] bool empty() const noexcept {
    return timer_count() == 0;
  }

  /**
   * @brief Get the tick of the next timer to fire (for sleep optimization)
   *
   * @return Next timer tick, or UINT64_MAX if no timers scheduled
   */
  [[nodiscard]] uint64_t next_timer_tick() const {
    if (empty()) {
      return UINT64_MAX;
    }

    // Check level 0 first (most common case)
    for (size_t offset = 0; offset < SLOTS_PER_LEVEL; ++offset) {
      size_t slot = (current_tick_ + offset) & (SLOTS_PER_LEVEL - 1);
      if (wheels_[0][slot] != nullptr) {
        return current_tick_ + offset;
      }
    }

    // Check higher levels
    for (size_t level = 1; level < NUM_LEVELS; ++level) {
      for (size_t slot = 0; slot < SLOTS_PER_LEVEL; ++slot) {
        if (wheels_[level][slot] != nullptr) {
          // Find minimum expiration in this slot
          uint64_t min_exp = UINT64_MAX;
          TimerEntry* entry = wheels_[level][slot];
          while (entry != nullptr) {
            if (entry->expiration_tick < min_exp) {
              min_exp = entry->expiration_tick;
            }
            entry = entry->next;
          }
          return min_exp;
        }
      }
    }

    return UINT64_MAX;
  }

  /**
   * @brief Get statistics about timer distribution
   */
  struct Stats {
    size_t timers_per_level[NUM_LEVELS];
    size_t total_timers;
    uint64_t current_tick;
  };

  [[nodiscard]] Stats get_stats() const {
    Stats stats{};
    stats.current_tick = current_tick_;
    stats.total_timers = 0;

    for (size_t level = 0; level < NUM_LEVELS; ++level) {
      stats.timers_per_level[level] = 0;
      for (size_t slot = 0; slot < SLOTS_PER_LEVEL; ++slot) {
        TimerEntry* entry = wheels_[level][slot];
        while (entry != nullptr) {
          stats.timers_per_level[level]++;
          entry = entry->next;
        }
      }
      stats.total_timers += stats.timers_per_level[level];
    }

    return stats;
  }

private:
  /**
   * @brief Insert a timer into the appropriate wheel level and slot
   */
  void insert_timer(TimerEntry* entry) {
    uint64_t delta = entry->expiration_tick - current_tick_;

    // Find the appropriate level
    size_t level = 0;
    for (size_t l = 0; l < NUM_LEVELS; ++l) {
      if (delta < LEVEL_RANGE[l]) {
        level = l;
        break;
      }
      if (l == NUM_LEVELS - 1) {
        level = NUM_LEVELS - 1;
      }
    }

    // Calculate slot within the level
    size_t slot;
    if (level == 0) {
      slot = entry->expiration_tick & (SLOTS_PER_LEVEL - 1);
    } else {
      slot = (entry->expiration_tick >> (BITS_PER_LEVEL * level)) & (SLOTS_PER_LEVEL - 1);
    }

    // Insert at head of slot's list
    entry->next = wheels_[level][slot];
    entry->prev = nullptr;
    if (wheels_[level][slot] != nullptr) {
      wheels_[level][slot]->prev = entry;
    }
    wheels_[level][slot] = entry;
  }

  /**
   * @brief Remove a timer from its slot
   */
  void remove_from_slot(TimerEntry* entry, size_t level, size_t slot) {
    if (entry->prev != nullptr) {
      entry->prev->next = entry->next;
    } else {
      wheels_[level][slot] = entry->next;
    }
    if (entry->next != nullptr) {
      entry->next->prev = entry->prev;
    }
  }

  /**
   * @brief Fire all timers in a slot
   */
  size_t fire_slot(size_t level, size_t slot) {
    size_t fired = 0;
    TimerEntry* entry = wheels_[level][slot];
    wheels_[level][slot] = nullptr;

    while (entry != nullptr) {
      TimerEntry* next = entry->next;

      if (entry->expiration_tick <= current_tick_) {
        // Timer has expired, fire it
        entry->callback();
        timer_count_.fetch_sub(1, std::memory_order_relaxed);
        delete entry;
        fired++;
      } else {
        // Timer not yet expired, re-insert (shouldn't happen in level 0)
        entry->next = nullptr;
        entry->prev = nullptr;
        insert_timer(entry);
      }

      entry = next;
    }

    return fired;
  }

  /**
   * @brief Cascade timers from a higher level slot down to lower levels
   */
  void cascade_slot(size_t level, size_t slot) {
    TimerEntry* entry = wheels_[level][slot];
    wheels_[level][slot] = nullptr;

    while (entry != nullptr) {
      TimerEntry* next = entry->next;
      entry->next = nullptr;
      entry->prev = nullptr;
      insert_timer(entry);
      entry = next;
    }
  }

  // Timer wheel storage: [level][slot] -> linked list of timers
  TimerEntry* wheels_[NUM_LEVELS][SLOTS_PER_LEVEL];

  uint64_t current_tick_;
  uint64_t next_timer_id_;
  std::atomic<size_t> timer_count_{0};
};

} // namespace veloz::core
