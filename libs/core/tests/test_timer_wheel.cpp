#include "kj/test.h"
#include "veloz/core/timer_wheel.h"

#include <atomic>
#include <kj/function.h>
#include <vector>

using namespace veloz::core;

namespace {

// Helper to create kj::Function from lambda
template <typename F> kj::Function<void()> makeCallback(F&& f) {
  return kj::Function<void()>(kj::fwd<F>(f));
}

// ============================================================================
// Basic Timer Wheel Tests
// ============================================================================

KJ_TEST("HierarchicalTimerWheel: Initial state") {
  HierarchicalTimerWheel wheel;

  KJ_EXPECT(wheel.current_tick() == 0);
  KJ_EXPECT(wheel.timer_count() == 0);
  KJ_EXPECT(wheel.empty());
  KJ_EXPECT(wheel.next_timer_tick() == UINT64_MAX);
}

KJ_TEST("HierarchicalTimerWheel: Schedule and fire single timer") {
  HierarchicalTimerWheel wheel;

  bool fired = false;
  wheel.schedule(5, makeCallback([&fired]() { fired = true; }));

  KJ_EXPECT(wheel.timer_count() == 1);
  KJ_EXPECT(!wheel.empty());

  // Advance 5 ticks (processes ticks 0-4) - timer should not fire yet
  for (int i = 0; i < 5; ++i) {
    wheel.tick();
    KJ_EXPECT(!fired);
  }

  // Advance 1 more tick (processes tick 5) - timer should fire
  wheel.tick();
  KJ_EXPECT(fired);
  KJ_EXPECT(wheel.empty());
}

KJ_TEST("HierarchicalTimerWheel: Schedule with zero delay") {
  HierarchicalTimerWheel wheel;

  bool fired = false;
  wheel.schedule(0, makeCallback([&fired]() { fired = true; }));

  KJ_EXPECT(wheel.timer_count() == 1);

  // Should fire on first tick
  wheel.tick();
  KJ_EXPECT(fired);
  KJ_EXPECT(wheel.empty());
}

KJ_TEST("HierarchicalTimerWheel: Multiple timers same slot") {
  HierarchicalTimerWheel wheel;

  int count = 0;
  wheel.schedule(5, makeCallback([&count]() { count++; }));
  wheel.schedule(5, makeCallback([&count]() { count++; }));
  wheel.schedule(5, makeCallback([&count]() { count++; }));

  KJ_EXPECT(wheel.timer_count() == 3);

  // Need 6 ticks to fire timers at delay 5 (process tick 5)
  wheel.advance(6);
  KJ_EXPECT(count == 3);
  KJ_EXPECT(wheel.empty());
}

KJ_TEST("HierarchicalTimerWheel: Timers fire in order") {
  HierarchicalTimerWheel wheel;

  std::vector<int> order;
  wheel.schedule(3, makeCallback([&order]() { order.push_back(1); }));
  wheel.schedule(1, makeCallback([&order]() { order.push_back(2); }));
  wheel.schedule(2, makeCallback([&order]() { order.push_back(3); }));

  // Need 4 ticks to fire all timers (process ticks 0,1,2,3)
  wheel.advance(4);

  KJ_EXPECT(order.size() == 3);
  KJ_EXPECT(order[0] == 2); // delay 1 fires at tick 1
  KJ_EXPECT(order[1] == 3); // delay 2 fires at tick 2
  KJ_EXPECT(order[2] == 1); // delay 3 fires at tick 3
}

KJ_TEST("HierarchicalTimerWheel: Cancel timer") {
  HierarchicalTimerWheel wheel;

  bool fired = false;
  uint64_t id = wheel.schedule(5, makeCallback([&fired]() { fired = true; }));

  KJ_EXPECT(wheel.timer_count() == 1);

  bool cancelled = wheel.cancel(id);
  KJ_EXPECT(cancelled);
  KJ_EXPECT(wheel.empty());

  wheel.advance(10);
  KJ_EXPECT(!fired);
}

KJ_TEST("HierarchicalTimerWheel: Cancel non-existent timer") {
  HierarchicalTimerWheel wheel;

  bool cancelled = wheel.cancel(12345);
  KJ_EXPECT(!cancelled);
}

KJ_TEST("HierarchicalTimerWheel: Cancel one of multiple timers") {
  HierarchicalTimerWheel wheel;

  int count = 0;
  wheel.schedule(5, makeCallback([&count]() { count++; }));
  uint64_t id2 = wheel.schedule(5, makeCallback([&count]() { count += 10; }));
  wheel.schedule(5, makeCallback([&count]() { count += 100; }));

  KJ_EXPECT(wheel.timer_count() == 3);

  wheel.cancel(id2);
  KJ_EXPECT(wheel.timer_count() == 2);

  // Need 6 ticks to fire timers at delay 5
  wheel.advance(6);
  KJ_EXPECT(count == 101); // 1 + 100, not 10
}

// ============================================================================
// Level 0 Tests (1ms resolution, 256ms range)
// ============================================================================

KJ_TEST("HierarchicalTimerWheel: Level 0 boundary") {
  HierarchicalTimerWheel wheel;

  bool fired = false;
  wheel.schedule(255, makeCallback([&fired]() { fired = true; }));

  // Need 256 ticks to fire timer at delay 255
  wheel.advance(255);
  KJ_EXPECT(!fired);

  wheel.tick();
  KJ_EXPECT(fired);
}

KJ_TEST("HierarchicalTimerWheel: Level 0 wraparound") {
  HierarchicalTimerWheel wheel;

  // Advance past one full rotation
  wheel.advance(300);

  bool fired = false;
  wheel.schedule(100, makeCallback([&fired]() { fired = true; }));

  // Need 101 ticks to fire timer at delay 100
  wheel.advance(100);
  KJ_EXPECT(!fired);

  wheel.tick();
  KJ_EXPECT(fired);
}

// ============================================================================
// Level 1 Tests (256ms resolution, ~65s range)
// ============================================================================

KJ_TEST("HierarchicalTimerWheel: Level 1 timer") {
  HierarchicalTimerWheel wheel;

  bool fired = false;
  // Schedule at 300ms (beyond level 0 range of 256ms)
  wheel.schedule(300, makeCallback([&fired]() { fired = true; }));

  // Timer should be in level 1
  auto stats = wheel.get_stats();
  KJ_EXPECT(stats.timers_per_level[1] == 1);

  // Need 301 ticks to fire timer at delay 300
  wheel.advance(300);
  KJ_EXPECT(!fired);

  wheel.tick();
  KJ_EXPECT(fired);
}

KJ_TEST("HierarchicalTimerWheel: Level 1 cascade") {
  HierarchicalTimerWheel wheel;

  bool fired = false;
  wheel.schedule(512, makeCallback([&fired]() { fired = true; }));

  // Should be in level 1 initially (slot 2)
  auto stats = wheel.get_stats();
  KJ_EXPECT(stats.timers_per_level[1] == 1);

  // Advance 513 ticks to process tick 512 (which triggers cascade of slot 2)
  // and then fire the timer at slot 0 of level 0
  wheel.advance(513);

  // Timer should have fired
  KJ_EXPECT(fired);
  KJ_EXPECT(wheel.empty());
}

// ============================================================================
// Level 2 Tests (~65s resolution, ~4.6 hour range)
// ============================================================================

KJ_TEST("HierarchicalTimerWheel: Level 2 timer") {
  HierarchicalTimerWheel wheel;

  bool fired = false;
  // Schedule at 70000ms (~70 seconds, beyond level 1 range of ~65s)
  wheel.schedule(70000, makeCallback([&fired]() { fired = true; }));

  auto stats = wheel.get_stats();
  KJ_EXPECT(stats.timers_per_level[2] == 1);

  // Need 70001 ticks to fire timer at delay 70000
  wheel.advance(70000);
  KJ_EXPECT(!fired);

  wheel.tick();
  KJ_EXPECT(fired);
}

// ============================================================================
// Statistics Tests
// ============================================================================

KJ_TEST("HierarchicalTimerWheel: Stats accuracy") {
  HierarchicalTimerWheel wheel;

  // Schedule timers at different levels
  wheel.schedule(10, makeCallback([]() {}));    // Level 0
  wheel.schedule(100, makeCallback([]() {}));   // Level 0
  wheel.schedule(300, makeCallback([]() {}));   // Level 1
  wheel.schedule(500, makeCallback([]() {}));   // Level 1
  wheel.schedule(70000, makeCallback([]() {})); // Level 2

  auto stats = wheel.get_stats();
  KJ_EXPECT(stats.timers_per_level[0] == 2);
  KJ_EXPECT(stats.timers_per_level[1] == 2);
  KJ_EXPECT(stats.timers_per_level[2] == 1);
  KJ_EXPECT(stats.total_timers == 5);
}

KJ_TEST("HierarchicalTimerWheel: Next timer tick") {
  HierarchicalTimerWheel wheel;

  KJ_EXPECT(wheel.next_timer_tick() == UINT64_MAX);

  wheel.schedule(50, makeCallback([]() {}));
  KJ_EXPECT(wheel.next_timer_tick() == 50);

  wheel.schedule(30, makeCallback([]() {}));
  KJ_EXPECT(wheel.next_timer_tick() == 30);

  // After advancing 31 ticks, timer at 30 fires, next is at 50
  wheel.advance(31);
  KJ_EXPECT(wheel.next_timer_tick() == 50);
}

// ============================================================================
// KJ Duration Integration Tests
// ============================================================================

KJ_TEST("HierarchicalTimerWheel: Schedule with KJ Duration") {
  HierarchicalTimerWheel wheel;

  bool fired = false;
  wheel.schedule(100 * kj::MILLISECONDS, makeCallback([&fired]() { fired = true; }));

  // Timer at delay 100 fires when we process tick 100
  // Need 101 ticks to fire timer at delay 100
  wheel.advance(101);
  KJ_EXPECT(fired);
}

KJ_TEST("HierarchicalTimerWheel: Schedule with seconds") {
  HierarchicalTimerWheel wheel;

  bool fired = false;
  wheel.schedule(1 * kj::SECONDS, makeCallback([&fired]() { fired = true; }));

  // 1 second = 1000ms, need 1001 ticks to fire timer at delay 1000
  wheel.advance(1001);
  KJ_EXPECT(fired);
}

// ============================================================================
// Performance Tests
// ============================================================================

KJ_TEST("HierarchicalTimerWheel: Many timers performance") {
  HierarchicalTimerWheel wheel;

  constexpr int NUM_TIMERS = 10000;
  std::atomic<int> fired_count{0};

  // Schedule many timers with delays 1-100 (all in level 0)
  for (int i = 0; i < NUM_TIMERS; ++i) {
    wheel.schedule((i % 100) + 1, makeCallback([&fired_count]() { fired_count++; }));
  }

  KJ_EXPECT(wheel.timer_count() == NUM_TIMERS);

  // Fire all timers (need 101 ticks to fire timers at delay 100)
  wheel.advance(101);

  KJ_EXPECT(fired_count.load() == NUM_TIMERS);
  KJ_EXPECT(wheel.empty());
}

KJ_TEST("HierarchicalTimerWheel: Rapid schedule and cancel") {
  HierarchicalTimerWheel wheel;

  std::vector<uint64_t> ids;
  for (int i = 0; i < 1000; ++i) {
    ids.push_back(wheel.schedule(100, makeCallback([]() {})));
  }

  KJ_EXPECT(wheel.timer_count() == 1000);

  // Cancel half
  for (size_t i = 0; i < ids.size(); i += 2) {
    wheel.cancel(ids[i]);
  }

  KJ_EXPECT(wheel.timer_count() == 500);

  // Fire remaining (need 101 ticks to fire timers at delay 100)
  wheel.advance(101);
  KJ_EXPECT(wheel.empty());
}

KJ_TEST("HierarchicalTimerWheel: Mixed level timers") {
  HierarchicalTimerWheel wheel;

  std::atomic<int> count{0};

  // Schedule timers across all levels
  // Level 0: delays 10-109 (100 timers)
  // Level 1: delays 300-399 (100 timers)
  // Level 2: delays 70000-70099 (100 timers)
  for (int i = 0; i < 100; ++i) {
    wheel.schedule(10 + i, makeCallback([&count]() { count++; }));    // Level 0
    wheel.schedule(300 + i, makeCallback([&count]() { count++; }));   // Level 1
    wheel.schedule(70000 + i, makeCallback([&count]() { count++; })); // Level 2
  }

  KJ_EXPECT(wheel.timer_count() == 300);

  // Fire level 0 timers (delays 10-109, need 110 ticks)
  wheel.advance(110);
  KJ_EXPECT(count.load() == 100);

  // Fire level 1 timers (delays 300-399, need to reach tick 400)
  // Current tick is 110, need 290 more ticks
  wheel.advance(290);
  KJ_EXPECT(count.load() == 200);

  // Fire level 2 timers (delays 70000-70099, need to reach tick 70100)
  // Current tick is 400, need 69700 more ticks
  wheel.advance(69700);
  KJ_EXPECT(count.load() == 300);
  KJ_EXPECT(wheel.empty());
}

} // namespace
