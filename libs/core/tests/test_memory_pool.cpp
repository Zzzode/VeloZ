#include "kj/test.h"
#include "veloz/core/memory.h"
#include "veloz/core/memory_pool.h"

#include <format>
#include <future> // Kept for std::async/std::future in concurrent tests
#include <kj/string.h>
#include <kj/vector.h>
#include <list>
#include <thread>
#include <vector> // Kept for MemoryPool internal compatibility

using namespace veloz::core;

namespace {

// Test class for pool testing
struct TestObject {
  TestObject(int value = 0) : value_(value), allocated(true) {}
  ~TestObject() {
    allocated = false;
  }

  int value_;
  bool allocated;

  static int count;
  static int active_count;
};

int TestObject::count = 0;
int TestObject::active_count = 0;

// ============================================================================
// FixedSizeMemoryPool Tests
// ============================================================================

KJ_TEST("MemoryPool: Create and destroy") {
  FixedSizeMemoryPool<TestObject, 4> pool(1, 10);

  KJ_EXPECT(pool.total_blocks() == 4);
  KJ_EXPECT(pool.available_blocks() == 4);
}

KJ_TEST("MemoryPool: Allocate") {
  FixedSizeMemoryPool<TestObject, 4> pool(0, 10);
  auto obj = pool.create(42);
  KJ_EXPECT(obj != nullptr);
  KJ_EXPECT(obj->value_ == 42);

  KJ_EXPECT(pool.total_blocks() == 4);
  KJ_EXPECT(pool.available_blocks() == 3); // One used
}

KJ_TEST("MemoryPool: Allocate and destroy") {
  FixedSizeMemoryPool<TestObject, 4> pool(1, 10);

  {
    auto obj = pool.create(42);
    KJ_EXPECT(pool.available_blocks() == 3);
  }

  KJ_EXPECT(pool.available_blocks() == 4); // Returned to pool
}

KJ_TEST("MemoryPool: Multiple allocations") {
  FixedSizeMemoryPool<TestObject, 4> pool(0, 10);
  std::vector<decltype(pool.create(0))> objects;
  for (int i = 0; i < 4; ++i) {
    objects.push_back(pool.create(i));
    KJ_EXPECT(objects.back()->value_ == i);
  }

  KJ_EXPECT(pool.available_blocks() == 0);

  // One more allocation should trigger new block
  auto obj5 = pool.create(5);
  KJ_EXPECT(obj5 != nullptr);
  KJ_EXPECT(pool.available_blocks() == 3);
  KJ_EXPECT(pool.total_blocks() == 8); // Two blocks now
}

KJ_TEST("MemoryPool: Pool exhaustion") {
  FixedSizeMemoryPool<TestObject, 2> pool(0, 2); // Max 2 blocks = 4 objects max
  std::vector<decltype(pool.create(0))> objects;
  for (int i = 0; i < 4; ++i) {
    objects.push_back(pool.create(i));
  }

  KJ_EXPECT(pool.total_blocks() == 4); // At max

  // Pool exhaustion should throw std::bad_alloc
  bool threw = false;
  try {
    auto obj5 = pool.create(5);
  } catch (const std::bad_alloc&) {
    threw = true;
  }
  KJ_EXPECT(threw);
}

KJ_TEST("MemoryPool: Statistics") {
  FixedSizeMemoryPool<TestObject, 4> pool(1, 10);

  {
    auto obj1 = pool.create(1);
    auto obj2 = pool.create(2);

    // Check stats while objects are still alive
    KJ_EXPECT(pool.allocation_count() >= 2);
    KJ_EXPECT(pool.total_allocated_bytes() > 0);
  }

  // After objects are destroyed
  KJ_EXPECT(pool.deallocation_count() >= 2);
  KJ_EXPECT(pool.peak_allocated_bytes() > 0);
}

KJ_TEST("MemoryPool: Preallocate") {
  FixedSizeMemoryPool<TestObject, 4> pool(0, 10);
  pool.preallocate(8); // Preallocate 2 blocks

  KJ_EXPECT(pool.total_blocks() == 8);
  KJ_EXPECT(pool.available_blocks() == 8);
}

KJ_TEST("MemoryPool: Reset") {
  FixedSizeMemoryPool<TestObject, 4> pool(1, 10);

  {
    auto obj1 = pool.create(1);
    auto obj2 = pool.create(2);
  }

  pool.reset();

  KJ_EXPECT(pool.total_blocks() == 0);
  KJ_EXPECT(pool.available_blocks() == 0);
  KJ_EXPECT(pool.peak_allocated_bytes() == 0);
}

KJ_TEST("MemoryPool: Shrink to fit") {
  FixedSizeMemoryPool<TestObject, 4> pool(2, 10);

  // Allocate and deallocate objects
  {
    auto obj1 = pool.create(1);
    auto obj2 = pool.create(2);
    auto obj3 = pool.create(3);
    auto obj4 = pool.create(4);
    auto obj5 = pool.create(5);
  }

  size_t before_shrink = pool.total_blocks();
  pool.shrink_to_fit();
  size_t after_shrink = pool.total_blocks();

  KJ_EXPECT(after_shrink <= before_shrink);
}

// ============================================================================
// MemoryMonitor Tests
// ============================================================================

KJ_TEST("MemoryMonitor: Track allocation") {
  MemoryMonitor monitor;

  monitor.track_allocation("test_site", 100, 1);
  monitor.track_allocation("test_site", 200, 2);

  KJ_EXPECT(monitor.total_allocated_bytes() == 300);

  monitor.track_deallocation("test_site", 100, 1);
  KJ_EXPECT(monitor.total_allocated_bytes() == 200);
}

KJ_TEST("MemoryMonitor: Site statistics") {
  MemoryMonitor monitor;

  monitor.track_allocation("site1", 100, 1);
  monitor.track_allocation("site2", 200, 2);

  auto* stats1 = monitor.get_site_stats("site1");
  KJ_EXPECT(stats1 != nullptr);
  KJ_EXPECT(stats1->current_bytes == 100);
  KJ_EXPECT(stats1->object_count == 1);

  auto* stats2 = monitor.get_site_stats("site2");
  KJ_EXPECT(stats2 != nullptr);
  KJ_EXPECT(stats2->current_bytes == 200);
  KJ_EXPECT(stats2->object_count == 2);
}

KJ_TEST("MemoryMonitor: Peak tracking") {
  MemoryMonitor monitor;

  monitor.track_allocation("peak_test", 100);
  monitor.track_allocation("peak_test", 200); // Peak: 300
  monitor.track_deallocation("peak_test", 100);
  KJ_EXPECT(monitor.peak_allocated_bytes() == 300);
  KJ_EXPECT(monitor.total_allocated_bytes() == 200);
}

KJ_TEST("MemoryMonitor: Generate report") {
  MemoryMonitor monitor;

  monitor.track_allocation("site1", 100);
  monitor.track_allocation("site2", 200);

  std::string report = monitor.generate_report();
  KJ_EXPECT(!report.empty());
  KJ_EXPECT(report.find("Memory Usage Report") != std::string::npos);
  KJ_EXPECT(report.find("Total Allocated") != std::string::npos);
}

KJ_TEST("MemoryMonitor: Reset monitor") {
  MemoryMonitor monitor;

  monitor.track_allocation("test", 100);
  KJ_EXPECT(monitor.total_allocated_bytes() > 0);

  monitor.reset();
  KJ_EXPECT(monitor.total_allocated_bytes() == 0);
  KJ_EXPECT(monitor.active_sites() == 0);
}

KJ_TEST("MemoryMonitor: Alert threshold") {
  MemoryMonitor monitor;

  monitor.set_alert_threshold(1000);
  monitor.track_allocation("test", 500);
  KJ_EXPECT(!monitor.check_alert());

  monitor.track_allocation("test", 600);
  KJ_EXPECT(monitor.check_alert()); // 500 + 600 = 1100 > 1000
}

KJ_TEST("MemoryMonitor: All sites") {
  MemoryMonitor monitor;

  monitor.track_allocation("site1", 100);
  monitor.track_allocation("site2", 200);
  monitor.track_allocation("site3", 300);

  auto sites = monitor.get_all_sites();
  KJ_EXPECT(sites.size() == 3);
  KJ_EXPECT(sites.find("site1") != sites.end());
  KJ_EXPECT(sites.find("site2") != sites.end());
  KJ_EXPECT(sites.find("site3") != sites.end());
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

KJ_TEST("MemoryPool: Concurrent allocations") {
  FixedSizeMemoryPool<TestObject, 4> pool(4, 20);
  const int thread_count = 4;
  const int allocs_per_thread = 10;

  std::vector<std::future<void>> futures;
  for (int t = 0; t < thread_count; ++t) {
    futures.push_back(std::async(std::launch::async, [&pool, allocs_per_thread, t] {
      std::vector<decltype(pool.create(0))> objects;
      for (int i = 0; i < allocs_per_thread; ++i) {
        objects.push_back(pool.create(i));
        std::this_thread::sleep_for(std::chrono::microseconds(1));
      }
    }));
  }

  for (auto& future : futures) {
    future.wait();
  }

  // Verify pool statistics are consistent
  KJ_EXPECT(pool.allocation_count() == pool.deallocation_count());
}

KJ_TEST("MemoryPool: Concurrent monitor tracking") {
  MemoryMonitor monitor;
  const int thread_count = 4;
  const int tracks_per_thread = 10;

  std::vector<std::future<void>> futures;
  for (int t = 0; t < thread_count; ++t) {
    futures.push_back(std::async(std::launch::async, [&monitor, tracks_per_thread, t] {
      for (int i = 0; i < tracks_per_thread; ++i) {
        monitor.track_allocation(std::format("thread_{}", t), 100);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        monitor.track_deallocation(std::format("thread_{}", t), 100);
      }
    }));
  }

  for (auto& future : futures) {
    future.wait();
  }

  KJ_EXPECT(monitor.active_sites() == thread_count);
  KJ_EXPECT(monitor.total_allocation_count() == thread_count * tracks_per_thread);
  KJ_EXPECT(monitor.total_deallocation_count() == thread_count * tracks_per_thread);
}

// ============================================================================
// Global Memory Monitor Tests
// ============================================================================

KJ_TEST("GlobalMemoryMonitor: Access") {
  auto& monitor = global_memory_monitor();

  monitor.track_allocation("global_test", 100);
  KJ_EXPECT(monitor.total_allocated_bytes() > 0);

  monitor.reset();
}

// ============================================================================
// ObjectPool (from memory.h) Tests
// ============================================================================

KJ_TEST("ObjectPool: Basic") {
  ObjectPool<TestObject> pool(2, 10);

  {
    auto obj1 = pool.acquire(1);
    KJ_EXPECT(obj1 != nullptr);
    KJ_EXPECT(obj1->value_ == 1);
    KJ_EXPECT(pool.available() == 1);

    auto obj2 = pool.acquire(2);
    KJ_EXPECT(obj2 != nullptr);
    KJ_EXPECT(obj2->value_ == 2);
    KJ_EXPECT(pool.available() == 0);
  }

  KJ_EXPECT(pool.available() == 2);
}

KJ_TEST("ObjectPool: Preallocate") {
  ObjectPool<TestObject> pool(0, 10);

  KJ_EXPECT(pool.size() == 0);
  pool.preallocate(5);
  KJ_EXPECT(pool.size() == 5);
  KJ_EXPECT(pool.available() == 5);
}

KJ_TEST("ObjectPool: Clear") {
  ObjectPool<TestObject> pool(5, 10);

  pool.clear();
  KJ_EXPECT(pool.size() == 0);
  KJ_EXPECT(pool.available() == 0);
}

KJ_TEST("ThreadLocalPool: Basic") {
  ThreadLocalObjectPool<TestObject> pool(2, 10);

  {
    auto obj = pool.acquire(42);
    KJ_EXPECT(obj != nullptr);
    KJ_EXPECT(obj->value_ == 42);
  }

  // Object should be returned to thread-local pool
  // (Note: ThreadLocalObjectPool doesn't have an available() method)
}

// ============================================================================
// ArenaAllocator Tests (KJ Arena-based memory management)
// ============================================================================

KJ_TEST("ArenaAllocator: Basic allocation") {
  ArenaAllocator arena(1024);

  auto& obj = arena.allocate<TestObject>(42);
  KJ_EXPECT(obj.value_ == 42);
  KJ_EXPECT(obj.allocated);

  KJ_EXPECT(arena.allocation_count() == 1);
  KJ_EXPECT(arena.total_allocated_bytes() >= sizeof(TestObject));
}

KJ_TEST("ArenaAllocator: Multiple allocations") {
  ArenaAllocator arena(1024);

  auto& obj1 = arena.allocate<TestObject>(1);
  auto& obj2 = arena.allocate<TestObject>(2);
  auto& obj3 = arena.allocate<TestObject>(3);

  KJ_EXPECT(obj1.value_ == 1);
  KJ_EXPECT(obj2.value_ == 2);
  KJ_EXPECT(obj3.value_ == 3);

  KJ_EXPECT(arena.allocation_count() == 3);
}

KJ_TEST("ArenaAllocator: Array allocation") {
  ArenaAllocator arena(1024);

  auto arr = arena.allocateArray<int>(10);
  KJ_EXPECT(arr.size() == 10);

  for (size_t i = 0; i < arr.size(); ++i) {
    arr[i] = static_cast<int>(i * 2);
  }

  for (size_t i = 0; i < arr.size(); ++i) {
    KJ_EXPECT(arr[i] == static_cast<int>(i * 2));
  }

  KJ_EXPECT(arena.allocation_count() == 1);
  KJ_EXPECT(arena.total_allocated_bytes() >= sizeof(int) * 10);
}

KJ_TEST("ArenaAllocator: Own allocation") {
  ArenaAllocator arena(1024);

  {
    auto obj = arena.allocateOwn<TestObject>(99);
    KJ_EXPECT(obj->value_ == 99);
    KJ_EXPECT(obj->allocated);
  }
  // Object destroyed when Own goes out of scope

  KJ_EXPECT(arena.allocation_count() == 1);
}

KJ_TEST("ArenaAllocator: String copy") {
  ArenaAllocator arena(1024);

  kj::StringPtr original = "Hello, Arena!"_kj;
  kj::StringPtr copied = arena.copyString(original);

  KJ_EXPECT(copied == original);
  KJ_EXPECT(copied.begin() != original.begin()); // Different memory location

  KJ_EXPECT(arena.allocation_count() == 1);
}

KJ_TEST("ArenaAllocator: Value copy") {
  ArenaAllocator arena(1024);

  TestObject original(123);
  auto& copied = arena.copy(kj::mv(original));

  KJ_EXPECT(copied.value_ == 123);
  KJ_EXPECT(arena.allocation_count() == 1);
}

KJ_TEST("ArenaAllocator: Direct arena access") {
  ArenaAllocator arena(1024);

  // Access underlying KJ arena directly
  kj::Arena& kj_arena = arena.arena();
  auto& obj = kj_arena.allocate<int>(42);
  KJ_EXPECT(obj == 42);
}

KJ_TEST("ScopedArena: Basic usage") {
  ScopedArena arena(1024);

  auto& obj = arena->allocate<TestObject>(77);
  KJ_EXPECT(obj.value_ == 77);

  auto arr = arena->allocateArray<int>(5);
  KJ_EXPECT(arr.size() == 5);

  KJ_EXPECT(arena->allocation_count() == 2);
}

KJ_TEST("ScopedArena: Move semantics") {
  ScopedArena arena1(1024);
  arena1->allocate<TestObject>(1);

  ScopedArena arena2 = kj::mv(arena1);
  KJ_EXPECT(arena2->allocation_count() == 1);

  arena2->allocate<TestObject>(2);
  KJ_EXPECT(arena2->allocation_count() == 2);
}

KJ_TEST("ArenaAllocator: Large allocation") {
  ArenaAllocator arena(256); // Small initial chunk

  // Allocate more than initial chunk size
  auto arr = arena.allocateArray<char>(1024);
  KJ_EXPECT(arr.size() == 1024);

  // Fill the array
  for (size_t i = 0; i < arr.size(); ++i) {
    arr[i] = static_cast<char>(i % 256);
  }

  // Verify
  for (size_t i = 0; i < arr.size(); ++i) {
    KJ_EXPECT(arr[i] == static_cast<char>(i % 256));
  }
}

KJ_TEST("ArenaAllocator: Destructor ordering") {
  static int destruction_order[3] = {0, 0, 0};
  static int destruction_index = 0;

  struct OrderedDestructor {
    int id;
    explicit OrderedDestructor(int id) : id(id) {}
    ~OrderedDestructor() {
      if (destruction_index < 3) {
        destruction_order[destruction_index++] = id;
      }
    }
  };

  destruction_index = 0;

  {
    ArenaAllocator arena(1024);
    arena.allocate<OrderedDestructor>(1);
    arena.allocate<OrderedDestructor>(2);
    arena.allocate<OrderedDestructor>(3);
  }

  // KJ Arena destroys in reverse order of allocation
  KJ_EXPECT(destruction_order[0] == 3);
  KJ_EXPECT(destruction_order[1] == 2);
  KJ_EXPECT(destruction_order[2] == 1);
}

// ============================================================================
// KJ Memory Utilities Tests
// ============================================================================

KJ_TEST("makeOwn: Basic") {
  auto obj = makeOwn<TestObject>(42);
  KJ_EXPECT(obj->value_ == 42);
  KJ_EXPECT(obj->allocated);
}

KJ_TEST("makeArray: Basic") {
  auto arr = makeArray<int>(10);
  KJ_EXPECT(arr.size() == 10);

  for (size_t i = 0; i < arr.size(); ++i) {
    arr[i] = static_cast<int>(i);
  }

  for (size_t i = 0; i < arr.size(); ++i) {
    KJ_EXPECT(arr[i] == static_cast<int>(i));
  }
}

KJ_TEST("makeArray: Initializer list") {
  auto arr = makeArray<int>({1, 2, 3, 4, 5});
  KJ_EXPECT(arr.size() == 5);
  KJ_EXPECT(arr[0] == 1);
  KJ_EXPECT(arr[1] == 2);
  KJ_EXPECT(arr[2] == 3);
  KJ_EXPECT(arr[3] == 4);
  KJ_EXPECT(arr[4] == 5);
}

KJ_TEST("wrapNonOwning: Basic") {
  TestObject obj(99);
  auto wrapped = wrapNonOwning(&obj);

  KJ_EXPECT(wrapped->value_ == 99);
  KJ_EXPECT(wrapped.get() == &obj);

  // wrapped goes out of scope but obj is NOT deleted
  // because wrapNonOwning uses NullDisposer
}

} // namespace
