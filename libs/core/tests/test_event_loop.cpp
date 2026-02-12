#include "veloz/core/event_loop.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

using namespace veloz::core;

class EventLoopTest : public ::testing::Test {
protected:
  void SetUp() override {
    loop_ = std::make_unique<EventLoop>();
  }

  void TearDown() override {
    if (loop_ && loop_->is_running()) {
      loop_->stop();
    }
  }

  std::unique_ptr<EventLoop> loop_;
  std::atomic<int> counter_{0};
};

// ============================================================================
// Basic Task Posting Tests
// ============================================================================

TEST_F(EventLoopTest, PostAndRunBasicTask) {
  bool task_executed = false;
  loop_->post([&] { task_executed = true; });

  std::thread worker([&] { loop_->run(); });
  loop_->stop();
  worker.join();

  EXPECT_TRUE(task_executed);
}

TEST_F(EventLoopTest, PostMultipleTasks) {
  const int task_count = 10;
  std::atomic<int> executed{0};

  for (int i = 0; i < task_count; ++i) {
    loop_->post([&] { ++executed; });
  }

  std::thread worker([&] { loop_->run(); });
  loop_->stop();
  worker.join();

  EXPECT_EQ(executed.load(), task_count);
}

TEST_F(EventLoopTest, PostDelayedTask) {
  const auto delay = std::chrono::milliseconds(100);
  std::atomic<bool> task_executed{false};
  auto start_time = std::chrono::steady_clock::now();

  loop_->post_delayed(
      [&] {
        task_executed = true;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start_time)
                           .count();
        EXPECT_GE(elapsed, delay.count() - 10); // Allow small tolerance
      },
      delay);

  std::thread worker([&] { loop_->run(); });

  std::this_thread::sleep_for(delay + std::chrono::milliseconds(50));
  loop_->stop();
  worker.join();

  EXPECT_TRUE(task_executed);
}

// ============================================================================
// Priority Tests
// ============================================================================

TEST_F(EventLoopTest, PostWithPriority) {
  std::vector<int> execution_order;
  std::mutex order_mutex;

  loop_->post(
      [&] {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(1);
      },
      EventPriority::Low);

  loop_->post(
      [&] {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(2);
      },
      EventPriority::Critical);

  loop_->post(
      [&] {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(3);
      },
      EventPriority::Normal);

  loop_->post(
      [&] {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(4);
      },
      EventPriority::High);

  std::thread worker([&] { loop_->run(); });
  loop_->stop();
  worker.join();

  // Critical should execute before High, before Normal, before Low
  ASSERT_EQ(execution_order.size(), 4);
  EXPECT_EQ(execution_order[0], 2); // Critical
  EXPECT_EQ(execution_order[1], 4); // High
  EXPECT_EQ(execution_order[2], 3); // Normal
  EXPECT_EQ(execution_order[3], 1); // Low
}

TEST_F(EventLoopTest, PostDelayedWithPriority) {
  std::vector<int> execution_order;
  std::mutex order_mutex;
  const auto delay = std::chrono::milliseconds(50);

  // Post delayed tasks with different priorities
  loop_->post_delayed(
      [&] {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(1);
      },
      delay, EventPriority::Low);

  loop_->post_delayed(
      [&] {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(2);
      },
      delay, EventPriority::Critical);

  std::thread worker([&] { loop_->run(); });

  std::this_thread::sleep_for(delay + std::chrono::milliseconds(50));
  loop_->stop();
  worker.join();

  ASSERT_EQ(execution_order.size(), 2);
  EXPECT_EQ(execution_order[0], 2); // Critical
  EXPECT_EQ(execution_order[1], 1); // Low
}

// ============================================================================
// Tag Tests
// ============================================================================

TEST_F(EventLoopTest, PostWithTags) {
  std::vector<std::string> received_tags;
  std::mutex tags_mutex;

  loop_->post_with_tags(
      [&] {
        // Task with tags
      },
      {"market", "binance"});

  std::thread worker([&] { loop_->run(); });
  loop_->stop();
  worker.join();

  // Tags are for filtering, we just verify they don't cause issues
  SUCCEED();
}

// ============================================================================
// Filter Tests
// ============================================================================

TEST_F(EventLoopTest, AddRemoveFilter) {
  std::atomic<int> normal_executed{0};
  std::atomic<int> low_executed{0};

  // Filter out all Low priority tasks
  uint64_t filter_id = loop_->add_filter(
      [](const std::vector<EventTag>&) {
        return false; // Filter out all
      },
      EventPriority::Low);

  loop_->post([&] { ++normal_executed; }, EventPriority::Normal);
  loop_->post([&] { ++low_executed; }, EventPriority::Low);

  std::thread worker([&] { loop_->run(); });
  loop_->stop();
  worker.join();

  EXPECT_EQ(normal_executed.load(), 1);
  EXPECT_EQ(low_executed.load(), 0); // Should be filtered out

  loop_->remove_filter(filter_id);

  // Reset and run again
  normal_executed = 0;
  low_executed = 0;

  loop_->post([&] { ++normal_executed; }, EventPriority::Normal);
  loop_->post([&] { ++low_executed; }, EventPriority::Low);

  std::thread worker2([&] { loop_->run(); });
  loop_->stop();
  worker2.join();

  EXPECT_EQ(normal_executed.load(), 1);
  EXPECT_EQ(low_executed.load(), 1); // Now should execute
}

TEST_F(EventLoopTest, TagFilter) {
  std::atomic<int> allowed_executed{0};
  std::atomic<int> filtered_executed{0};

  // Filter out tasks with tag "debug"
  (void)loop_->add_tag_filter("debug.*");

  loop_->post_with_tags([&] { ++allowed_executed; }, {"market", "trade"});
  loop_->post_with_tags([&] { ++filtered_executed; }, {"debug", "trace"});

  std::thread worker([&] { loop_->run(); });
  loop_->stop();
  worker.join();

  EXPECT_EQ(allowed_executed.load(), 1);
  EXPECT_EQ(filtered_executed.load(), 0);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(EventLoopTest, StatisticsTracking) {
  const int task_count = 5;

  for (int i = 0; i < task_count; ++i) {
    loop_->post([&] {}, EventPriority::Normal);
  }

  loop_->post([&] {}, EventPriority::Low);
  loop_->post([&] {}, EventPriority::High);

  std::thread worker([&] { loop_->run(); });
  loop_->stop();
  worker.join();

  const auto& stats = loop_->stats();
  EXPECT_GT(stats.total_events.load(), 0);
  EXPECT_GT(stats.events_processed.load(), 0);
  EXPECT_GT(stats.events_by_priority[static_cast<int>(EventPriority::Normal)].load(), 0);
  EXPECT_GT(stats.events_by_priority[static_cast<int>(EventPriority::Low)].load(), 0);
  EXPECT_GT(stats.events_by_priority[static_cast<int>(EventPriority::High)].load(), 0);
}

TEST_F(EventLoopTest, StatsToString) {
  std::string stats = loop_->stats_to_string();
  EXPECT_FALSE(stats.empty());
  EXPECT_TRUE(stats.find("EventLoop Statistics") != std::string::npos);
}

// ============================================================================
// Status Tests
// ============================================================================

TEST_F(EventLoopTest, IsRunning) {
  std::atomic<bool> running_started{false};

  std::thread worker([&] {
    running_started = true;
    EXPECT_TRUE(loop_->is_running());
    loop_->run();
  });

  // Give thread time to start
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_TRUE(running_started.load());

  loop_->stop();
  worker.join();

  EXPECT_FALSE(loop_->is_running());
}

TEST_F(EventLoopTest, PendingTasks) {
  loop_->post([&] {});
  loop_->post([&] {});
  loop_->post_delayed([&] {}, std::chrono::milliseconds(100));

  EXPECT_EQ(loop_->pending_tasks(), 3);
}

// ============================================================================
// Routing Tests
// ============================================================================

TEST_F(EventLoopTest, SetRouter) {
  std::vector<std::string> routes;
  std::mutex routes_mutex;

  loop_->set_router([&](const std::vector<EventTag>& tags, std::function<void()> task) {
    std::lock_guard<std::mutex> lock(routes_mutex);
    std::string tag_str;
    for (const auto& tag : tags) {
      tag_str += tag + ",";
    }
    routes.push_back(tag_str);
    task();
  });

  loop_->post_with_tags([&] {}, EventPriority::Normal, {"route1"});
  loop_->post_with_tags([&] {}, EventPriority::Normal, {"route2"});

  std::thread worker([&] { loop_->run(); });
  loop_->stop();
  worker.join();

  ASSERT_EQ(routes.size(), 2);
  EXPECT_TRUE(routes[0].find("route1") != std::string::npos);
  EXPECT_TRUE(routes[1].find("route2") != std::string::npos);
}

// ============================================================================
// Helper Function Tests
// ============================================================================

TEST(ToEventPriorityString, AllLevels) {
  EXPECT_EQ(to_string(EventPriority::Low), "Low");
  EXPECT_EQ(to_string(EventPriority::Normal), "Normal");
  EXPECT_EQ(to_string(EventPriority::High), "High");
  EXPECT_EQ(to_string(EventPriority::Critical), "Critical");
}
