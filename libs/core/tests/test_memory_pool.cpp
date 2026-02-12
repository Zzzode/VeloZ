#include <gtest/gtest.h>

#include "veloz/core/memory_pool.h"
#include "veloz/core/memory.h"

#include <vector>
#include <thread>
#include <future>
#include <format>
#include <list>

using namespace veloz::core;

class MemoryPoolTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

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

TEST_F(MemoryPoolTest, CreateAndDestroy) {
  FixedSizeMemoryPool<TestObject, 4> pool(1, 10);

  EXPECT_EQ(pool.total_blocks(), 4);
  EXPECT_EQ(pool.available_blocks(), 4);
}

TEST_F(MemoryPoolTest, Allocate) {
  FixedSizeMemoryPool<TestObject, 4> pool(0, 10);

  auto obj = pool.create(42);
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->value_, 42);

  EXPECT_EQ(pool.total_blocks(), 4);
  EXPECT_EQ(pool.available_blocks(), 3); // One used
}

TEST_F(MemoryPoolTest, AllocateAndDestroy) {
  FixedSizeMemoryPool<TestObject, 4> pool(1, 10);

  {
    auto obj = pool.create(42);
    EXPECT_EQ(pool.available_blocks(), 3);
  }

  EXPECT_EQ(pool.available_blocks(), 4); // Returned to pool
}

TEST_F(MemoryPoolTest, MultipleAllocations) {
  FixedSizeMemoryPool<TestObject, 4> pool(0, 10);

  std::vector<decltype(pool.create(0))> objects;
  for (int i = 0; i < 4; ++i) {
    objects.push_back(pool.create(i));
    EXPECT_EQ(objects.back()->value_, i);
  }

  EXPECT_EQ(pool.available_blocks(), 0);

  // One more allocation should trigger new block
  auto obj5 = pool.create(5);
  EXPECT_NE(obj5, nullptr);
  EXPECT_EQ(pool.available_blocks(), 3);
  EXPECT_EQ(pool.total_blocks(), 8); // Two blocks now
}

TEST_F(MemoryPoolTest, PoolExhaustion) {
  FixedSizeMemoryPool<TestObject, 2> pool(0, 2); // Max 2 blocks = 4 objects max

  std::vector<decltype(pool.create(0))> objects;
  for (int i = 0; i < 4; ++i) {
    objects.push_back(pool.create(i));
  }

  EXPECT_EQ(pool.total_blocks(), 4); // At max
  EXPECT_THROW(pool.create(5), std::bad_alloc);
}

TEST_F(MemoryPoolTest, Statistics) {
  FixedSizeMemoryPool<TestObject, 4> pool(1, 10);

  {
    auto obj1 = pool.create(1);
    auto obj2 = pool.create(2);
  }

  EXPECT_GT(pool.allocation_count(), 0);
  EXPECT_GT(pool.deallocation_count(), 0);
  EXPECT_GT(pool.total_allocated_bytes(), 0);
}

TEST_F(MemoryPoolTest, Preallocate) {
  FixedSizeMemoryPool<TestObject, 4> pool(0, 10);

  pool.preallocate(8); // Preallocate 2 blocks

  EXPECT_EQ(pool.total_blocks(), 8);
  EXPECT_EQ(pool.available_blocks(), 8);
}

TEST_F(MemoryPoolTest, Reset) {
  FixedSizeMemoryPool<TestObject, 4> pool(1, 10);

  {
    auto obj1 = pool.create(1);
    auto obj2 = pool.create(2);
  }

  pool.reset();

  EXPECT_EQ(pool.total_blocks(), 0);
  EXPECT_EQ(pool.available_blocks(), 0);
  EXPECT_EQ(pool.peak_allocated_bytes(), 0);
}

TEST_F(MemoryPoolTest, ShrinkToFit) {
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

  EXPECT_LE(after_shrink, before_shrink);
}

// ============================================================================
// PoolAllocator Tests
// ============================================================================

// Note: PoolAllocator tests disabled - the PoolAllocator implementation
// uses a static default pool and doesn't support custom pool construction

// ============================================================================
// MemoryMonitor Tests
// ============================================================================

TEST_F(MemoryPoolTest, TrackAllocation) {
  MemoryMonitor monitor;

  monitor.track_allocation("test_site", 100, 1);
  monitor.track_allocation("test_site", 200, 2);

  EXPECT_EQ(monitor.total_allocated_bytes(), 300);

  monitor.track_deallocation("test_site", 100, 1);

  EXPECT_EQ(monitor.total_allocated_bytes(), 200);
}

TEST_F(MemoryPoolTest, SiteStatistics) {
  MemoryMonitor monitor;

  monitor.track_allocation("site1", 100, 1);
  monitor.track_allocation("site2", 200, 2);

  auto* stats1 = monitor.get_site_stats("site1");
  ASSERT_NE(stats1, nullptr);
  EXPECT_EQ(stats1->current_bytes, 100);
  EXPECT_EQ(stats1->object_count, 1);

  auto* stats2 = monitor.get_site_stats("site2");
  ASSERT_NE(stats2, nullptr);
  EXPECT_EQ(stats2->current_bytes, 200);
  EXPECT_EQ(stats2->object_count, 2);
}

TEST_F(MemoryPoolTest, PeakTracking) {
  MemoryMonitor monitor;

  monitor.track_allocation("peak_test", 100);
  monitor.track_allocation("peak_test", 200);  // Peak: 300
  monitor.track_deallocation("peak_test", 100);

  EXPECT_EQ(monitor.peak_allocated_bytes(), 300);
  EXPECT_EQ(monitor.total_allocated_bytes(), 200);
}

TEST_F(MemoryPoolTest, GenerateReport) {
  MemoryMonitor monitor;

  monitor.track_allocation("site1", 100);
  monitor.track_allocation("site2", 200);

  std::string report = monitor.generate_report();

  EXPECT_FALSE(report.empty());
  EXPECT_TRUE(report.find("Memory Usage Report") != std::string::npos);
  EXPECT_TRUE(report.find("Total Allocated") != std::string::npos);
}

TEST_F(MemoryPoolTest, ResetMonitor) {
  MemoryMonitor monitor;

  monitor.track_allocation("test", 100);
  EXPECT_GT(monitor.total_allocated_bytes(), 0);

  monitor.reset();
  EXPECT_EQ(monitor.total_allocated_bytes(), 0);
  EXPECT_EQ(monitor.active_sites(), 0);
}

TEST_F(MemoryPoolTest, AlertThreshold) {
  MemoryMonitor monitor;

  monitor.set_alert_threshold(1000);

  monitor.track_allocation("test", 500);
  EXPECT_FALSE(monitor.check_alert());

  monitor.track_allocation("test", 600);
  EXPECT_TRUE(monitor.check_alert()); // 500 + 600 = 1100 > 1000
}

TEST_F(MemoryPoolTest, AllSites) {
  MemoryMonitor monitor;

  monitor.track_allocation("site1", 100);
  monitor.track_allocation("site2", 200);
  monitor.track_allocation("site3", 300);

  auto sites = monitor.get_all_sites();
  EXPECT_EQ(sites.size(), 3);

  EXPECT_NE(sites.find("site1"), sites.end());
  EXPECT_NE(sites.find("site2"), sites.end());
  EXPECT_NE(sites.find("site3"), sites.end());
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(MemoryPoolTest, ConcurrentAllocations) {
  FixedSizeMemoryPool<TestObject, 4> pool(4, 20);
  const int thread_count = 4;
  const int allocs_per_thread = 10;

  std::vector<std::future<void>> futures;

  for (int t = 0; t < thread_count; ++t) {
    futures.push_back(std::async(std::launch::async, [&pool, allocs_per_thread] {
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
  EXPECT_EQ(pool.allocation_count(), pool.deallocation_count());
}

TEST_F(MemoryPoolTest, ConcurrentMonitorTracking) {
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

  EXPECT_EQ(monitor.active_sites(), thread_count);
  EXPECT_EQ(monitor.total_allocation_count(), thread_count * tracks_per_thread);
  EXPECT_EQ(monitor.total_deallocation_count(), thread_count * tracks_per_thread);
}

// ============================================================================
// Global Memory Monitor Tests
// ============================================================================

TEST(GlobalMemoryMonitor, Access) {
  auto& monitor = global_memory_monitor();

  monitor.track_allocation("global_test", 100);
  EXPECT_GT(monitor.total_allocated_bytes(), 0);

  monitor.reset();
}

// ============================================================================
// ObjectPool (from memory.h) Tests
// ============================================================================

TEST_F(MemoryPoolTest, ObjectPoolBasic) {
  ObjectPool<TestObject> pool(2, 10);

  {
    auto obj1 = pool.acquire(1);
    EXPECT_NE(obj1, nullptr);
    EXPECT_EQ(obj1->value_, 1);
    EXPECT_EQ(pool.available(), 1);

    auto obj2 = pool.acquire(2);
    EXPECT_NE(obj2, nullptr);
    EXPECT_EQ(obj2->value_, 2);
    EXPECT_EQ(pool.available(), 0);
  }

  EXPECT_EQ(pool.available(), 2);
}

TEST_F(MemoryPoolTest, ObjectPoolPreallocate) {
  ObjectPool<TestObject> pool(0, 10);

  EXPECT_EQ(pool.size(), 0);

  pool.preallocate(5);

  EXPECT_EQ(pool.size(), 5);
  EXPECT_EQ(pool.available(), 5);
}

TEST_F(MemoryPoolTest, ObjectPoolClear) {
  ObjectPool<TestObject> pool(5, 10);

  pool.clear();

  EXPECT_EQ(pool.size(), 0);
  EXPECT_EQ(pool.available(), 0);
}

// ============================================================================
// ThreadLocalObjectPool Tests
// ============================================================================

TEST_F(MemoryPoolTest, ThreadLocalPool) {
  ThreadLocalObjectPool<TestObject> pool(2, 10);

  {
    auto obj = pool.acquire(42);
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(obj->value_, 42);
  }

  // Object should be returned to thread-local pool
}

// Note: ThreadLocalPoolDifferentThreads test disabled
// ThreadLocalObjectPool doesn't have an available() method
