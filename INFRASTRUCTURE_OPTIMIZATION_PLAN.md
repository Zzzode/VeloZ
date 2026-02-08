# VeloZ 量化交易框架 - 基础架构完善方案

## 1. 概述

基础架构是量化交易框架的核心支撑，直接影响系统的性能、可靠性和可扩展性。通过对当前代码库的分析，我们发现了几个需要重点优化的领域：事件循环性能、日志系统、指标系统和内存管理。

## 2. 事件循环性能优化

### 2.1 当前实现分析

当前事件循环实现使用标准的互斥锁和条件变量：

```cpp
// 当前事件循环实现（简化版）
void EventLoop::post(std::function<void()> task) {
  {
    std::scoped_lock lock(mu_);
    tasks_.push(std::move(task));
  }
  cv_.notify_one();
}

void EventLoop::run() {
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock lock(mu_);

      // 处理延迟任务
      auto now = std::chrono::steady_clock::now();
      if (!delayed_tasks_.empty() && delayed_tasks_.top().deadline <= now) {
        task = std::move(delayed_tasks_.top().task);
        delayed_tasks_.pop();
      }
      // 处理普通任务
      else if (!tasks_.empty()) {
        task = std::move(tasks_.front());
        tasks_.pop();
      }
      // 等待任务
      else {
        cv_.wait(lock);
      }
    }

    if (task) {
      task();
    }
  }
}
```

**问题分析**：
1. 使用互斥锁保护任务队列，在高并发场景下可能导致锁竞争
2. 条件变量等待和唤醒机制存在一定的性能开销
3. 任务队列的实现使用标准容器，可能存在内存分配开销

### 2.2 优化方案

#### 2.2.1 无锁队列实现

```cpp
// 无锁任务队列
template <typename T>
class LockFreeQueue {
public:
  bool push(T value) {
    // 使用原子操作实现无锁入队
    // 简化示例，实际需要更复杂的实现
    auto new_node = new Node(std::move(value));
    Node* old_tail = tail_.load();
    while (!tail_.compare_exchange_weak(old_tail, new_node)) {
      old_tail = tail_.load();
    }
    old_tail->next = new_node;
    return true;
  }

  std::optional<T> pop() {
    // 使用原子操作实现无锁出队
    Node* old_head = head_.load();
    while (old_head->next) {
      if (head_.compare_exchange_weak(old_head, old_head->next)) {
        auto value = std::move(old_head->next->value);
        delete old_head;
        return value;
      }
    }
    return std::nullopt;
  }

private:
  struct Node {
    T value;
    Node* next{nullptr};
    explicit Node(T val) : value(std::move(val)) {}
  };

  std::atomic<Node*> head_{new Node(T{})};
  std::atomic<Node*> tail_{head_.load()};
};

// 优化后的事件循环
class OptimizedEventLoop {
public:
  using Task = std::function<void()>;

  void post(Task task) {
    task_queue_.push(std::move(task));
    cv_.notify_one(); // 仍然需要通知，但锁竞争大大减少
  }

  void run() {
    for (;;) {
      std::optional<Task> task = task_queue_.pop();
      if (task) {
        (*task)();
      } else {
        // 无任务时等待
        std::unique_lock lock(mu_);
        cv_.wait(lock, [this]() { return !task_queue_.empty(); });
      }
    }
  }

private:
  LockFreeQueue<Task> task_queue_;
  std::mutex mu_;
  std::condition_variable cv_;
};
```

#### 2.2.2 任务批量处理

```cpp
// 任务批量处理优化
void OptimizedEventLoop::run() {
  std::vector<Task> tasks;
  tasks.reserve(64); // 预先分配空间，减少内存分配

  for (;;) {
    // 批量获取任务
    while (auto task = task_queue_.pop()) {
      tasks.push_back(std::move(*task));
      if (tasks.size() >= 64) {
        break; // 达到批量处理阈值
      }
    }

    // 处理任务
    for (auto& task : tasks) {
      task();
    }
    tasks.clear();

    // 无任务时等待
    if (task_queue_.empty()) {
      std::unique_lock lock(mu_);
      cv_.wait(lock, [this]() { return !task_queue_.empty(); });
    }
  }
}
```

## 3. 日志系统优化

### 3.1 当前实现分析

当前日志系统使用同步写入和互斥锁：

```cpp
// 当前日志实现
void Logger::log(LogLevel level, std::string_view message,
                 const std::source_location& location) {
  if (level < level_) {
    return;
  }

  std::scoped_lock lock(mu_);
  // 格式化日志消息
  auto now = std::chrono::system_clock::now();
  auto time_str = format_time(now);
  auto log_line = std::format("[{}] [{}] [{}:{}] {}",
                              time_str, to_string(level),
                              location.file_name(), location.line(), message);
  *out_ << log_line << std::endl;
}
```

**问题分析**：
1. 同步写入阻塞主事件循环
2. 格式化字符串操作在锁内执行，导致锁持有时间过长
3. 无缓冲机制，频繁的小日志写入会导致性能问题

### 3.2 优化方案

#### 3.2.1 异步日志系统

```cpp
// 异步日志系统
class AsyncLogger {
public:
  AsyncLogger(std::ostream& out)
    : out_(out), running_(true), worker_(std::thread([this]() { work(); })) {}

  ~AsyncLogger() {
    running_ = false;
    cv_.notify_one();
    if (worker_.joinable()) {
      worker_.join();
    }
    flush_remaining();
  }

  void log(LogLevel level, std::string_view message,
           const std::source_location& location) {
    if (level < level_) {
      return;
    }

    auto log_entry = create_log_entry(level, message, location);
    {
      std::scoped_lock lock(mu_);
      queue_.push(std::move(log_entry));
    }
    cv_.notify_one();
  }

private:
  struct LogEntry {
    std::string message;
    LogLevel level;
    std::source_location location;
    std::chrono::system_clock::time_point timestamp;
  };

  LogEntry create_log_entry(LogLevel level, std::string_view message,
                           const std::source_location& location) {
    return {
      std::string(message),
      level,
      location,
      std::chrono::system_clock::now()
    };
  }

  void work() {
    std::vector<LogEntry> entries;
    entries.reserve(64);

    while (running_) {
      {
        std::unique_lock lock(mu_);
        cv_.wait_for(lock, std::chrono::milliseconds(100),
                    [this]() { return !queue_.empty() || !running_; });

        // 批量获取日志条目
        while (!queue_.empty()) {
          entries.push_back(std::move(queue_.front()));
          queue_.pop();
        }
      }

      // 批量写入
      if (!entries.empty()) {
        write_entries(entries);
        entries.clear();
      }
    }
  }

  void write_entries(const std::vector<LogEntry>& entries) {
    for (const auto& entry : entries) {
      auto time_str = format_time(entry.timestamp);
      auto log_line = std::format("[{}] [{}] [{}:{}] {}",
                                  time_str, to_string(entry.level),
                                  entry.location.file_name(), entry.location.line(),
                                  entry.message);
      out_ << log_line << std::endl;
    }
    out_.flush();
  }

  void flush_remaining() {
    std::vector<LogEntry> entries;
    {
      std::scoped_lock lock(mu_);
      while (!queue_.empty()) {
        entries.push_back(std::move(queue_.front()));
        queue_.pop();
      }
    }
    if (!entries.empty()) {
      write_entries(entries);
    }
  }

  std::ostream& out_;
  LogLevel level_{LogLevel::Info};
  std::queue<LogEntry> queue_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::atomic<bool> running_;
  std::thread worker_;
};
```

#### 3.2.2 内存池优化

```cpp
// 日志消息内存池
class LogMessagePool {
public:
  using Buffer = std::vector<char>;

  std::shared_ptr<Buffer> acquire() {
    std::scoped_lock lock(mu_);
    if (!pool_.empty()) {
      auto buffer = std::move(pool_.back());
      pool_.pop_back();
      buffer->clear();
      return buffer;
    }
    return std::make_shared<Buffer>(4096); // 预分配4KB缓冲区
  }

  void release(std::shared_ptr<Buffer> buffer) {
    std::scoped_lock lock(mu_);
    if (pool_.size() < 100) { // 限制池大小
      pool_.push_back(std::move(buffer));
    }
  }

private:
  std::vector<std::shared_ptr<Buffer>> pool_;
  std::mutex mu_;
};

// 使用内存池的日志格式化
std::string format_log_message(const LogEntry& entry, LogMessagePool& pool) {
  auto buffer = pool.acquire();
  buffer->reserve(1024);

  auto time_str = format_time(entry.timestamp);
  fmt::format_to(std::back_inserter(*buffer), "[{}] [{}] [{}:{}] {}",
                 time_str, to_string(entry.level),
                 entry.location.file_name(), entry.location.line(),
                 entry.message);

  std::string result(buffer->data(), buffer->size());
  pool.release(buffer);
  return result;
}
```

## 4. 指标系统优化

### 4.1 当前实现分析

当前指标系统使用原子操作和互斥锁：

```cpp
// 当前指标注册表
class MetricsRegistry {
public:
  void register_counter(std::string name, std::string description) {
    std::scoped_lock lock(mu_);
    counters_[std::move(name)] = std::make_unique<Counter>(name, std::move(description));
  }

  Counter* counter(std::string_view name) {
    std::scoped_lock lock(mu_);
    auto it = counters_.find(std::string(name));
    return it != counters_.end() ? it->second.get() : nullptr;
  }

private:
  mutable std::mutex mu_;
  std::map<std::string, std::unique_ptr<Counter>> counters_;
  std::map<std::string, std::unique_ptr<Gauge>> gauges_;
  std::map<std::string, std::unique_ptr<Histogram>> histograms_;
};
```

**问题分析**：
1. 注册和获取指标时需要加锁，影响性能
2. 使用 std::map 存储指标，查询速度不如哈希表
3. 指标导出时需要遍历所有指标，可能导致锁竞争

### 4.2 优化方案

#### 4.2.1 无锁指标注册表

```cpp
// 无锁指标注册表
class LockFreeMetricsRegistry {
public:
  void register_counter(std::string name, std::string description) {
    auto counter = std::make_unique<Counter>(std::move(name), std::move(description));
    auto name_str = counter->name();

    // 使用原子操作插入
    std::unique_ptr<Counter> expected = nullptr;
    if (counters_.find(name_str) == counters_.end()) {
      counters_.try_emplace(name_str, std::move(counter));
    }
  }

  Counter* counter(std::string_view name) {
    auto it = counters_.find(std::string(name));
    return it != counters_.end() ? it->second.get() : nullptr;
  }

  // 其他指标类型的方法类似...

private:
  // 使用并发哈希表（如TBB的concurrent_hash_map）
  tbb::concurrent_hash_map<std::string, std::unique_ptr<Counter>> counters_;
  tbb::concurrent_hash_map<std::string, std::unique_ptr<Gauge>> gauges_;
  tbb::concurrent_hash_map<std::string, std::unique_ptr<Histogram>> histograms_;
};
```

#### 4.2.2 延迟初始化和缓存

```cpp
// 指标延迟初始化和缓存
class CachedMetricsRegistry {
public:
  Counter* counter(std::string_view name) {
    auto cache_key = std::string(name);
    auto it = counter_cache_.find(cache_key);
    if (it != counter_cache_.end()) {
      return it->second;
    }

    // 双重检查锁定
    std::scoped_lock lock(mu_);
    auto it2 = counters_.find(cache_key);
    if (it2 != counters_.end()) {
      auto ptr = it2->second.get();
      counter_cache_.emplace(cache_key, ptr);
      return ptr;
    }
    return nullptr;
  }

private:
  std::unordered_map<std::string, Counter*> counter_cache_;
  std::map<std::string, std::unique_ptr<Counter>> counters_;
  std::mutex mu_;
};
```

## 5. 内存管理优化

### 5.1 问题分析

当前项目中没有明确的内存管理策略，可能导致以下问题：
1. 频繁的内存分配和释放
2. 内存泄漏风险
3. 内存碎片

### 5.2 优化方案

#### 5.2.1 对象池

```cpp
// 通用对象池
template <typename T>
class ObjectPool {
public:
  using ObjectPtr = std::shared_ptr<T>;

  ObjectPtr acquire() {
    std::scoped_lock lock(mu_);
    if (!pool_.empty()) {
      auto obj = std::move(pool_.back());
      pool_.pop_back();
      return obj;
    }
    return create_object();
  }

  void release(ObjectPtr obj) {
    std::scoped_lock lock(mu_);
    if (pool_.size() < max_size_) {
      pool_.push_back(std::move(obj));
    }
  }

private:
  ObjectPtr create_object() {
    auto obj = std::make_shared<T>();
    // 使用自定义删除器，将对象返回池而不是删除
    return ObjectPtr(obj.get(), [this](T* ptr) {
      release(std::shared_ptr<T>(ptr));
    });
  }

  std::vector<ObjectPtr> pool_;
  std::mutex mu_;
  size_t max_size_{1000}; // 池的最大大小
};

// 预分配内存池
template <typename T>
class PreallocatedObjectPool {
public:
  PreallocatedObjectPool(size_t initial_size = 100) {
    pool_.reserve(initial_size);
    for (size_t i = 0; i < initial_size; ++i) {
      pool_.push_back(std::make_shared<T>());
    }
  }

  // 与ObjectPool类似的acquire和release方法
};
```

#### 5.2.2 内存分配器优化

```cpp
// 自定义内存分配器
template <typename T>
class QuantAllocator {
public:
  using value_type = T;

  QuantAllocator() = default;

  template <typename U>
  QuantAllocator(const QuantAllocator<U>&) {}

  T* allocate(std::size_t n) {
    // 使用预分配的内存块
    auto block = memory_pool_.allocate(n * sizeof(T));
    return static_cast<T*>(block);
  }

  void deallocate(T* p, std::size_t n) {
    memory_pool_.deallocate(static_cast<void*>(p), n * sizeof(T));
  }

private:
  static ThreadLocalMemoryPool memory_pool_;
};

// 线程本地内存池
class ThreadLocalMemoryPool {
public:
  void* allocate(size_t size);
  void deallocate(void* ptr, size_t size);

private:
  // 内存池实现...
};

// 使用自定义分配器的容器
using FastQueue = std::queue<MarketEvent, std::deque<MarketEvent, QuantAllocator<MarketEvent>>>;
```

## 6. 实施计划

### 6.1 阶段划分

1. **第一阶段（2周）**：事件循环性能优化
   - 实现无锁任务队列
   - 添加任务批量处理功能
   - 性能基准测试

2. **第二阶段（2周）**：日志系统优化
   - 实现异步日志系统
   - 添加内存池优化
   - 性能基准测试

3. **第三阶段（2周）**：指标系统优化
   - 实现无锁指标注册表
   - 添加延迟初始化和缓存
   - 性能基准测试

4. **第四阶段（2周）**：内存管理优化
   - 实现对象池
   - 添加自定义内存分配器
   - 内存泄漏检测和修复

### 6.2 测试和验证

```cpp
// 性能基准测试框架
class PerformanceBenchmark {
public:
  struct Result {
    std::string test_name;
    uint64_t operations_per_second;
    double avg_latency_ms;
    double max_latency_ms;
    double cpu_usage;
    double memory_usage;
  };

  template <typename Func>
  Result benchmark(const std::string& test_name, Func&& func, int iterations = 10000) {
    Timer timer;
    std::vector<double> latencies;
    latencies.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
      Timer op_timer;
      func();
      latencies.push_back(op_timer.elapsed_ms());
    }

    // 计算统计信息
    auto total_time = timer.elapsed_ms();
    auto avg_latency = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
    auto max_latency = *std::max_element(latencies.begin(), latencies.end());
    auto operations_per_second = (iterations * 1000.0) / total_time;

    return {
      test_name,
      static_cast<uint64_t>(operations_per_second),
      avg_latency,
      max_latency,
      get_cpu_usage(),
      get_memory_usage()
    };
  }
};

// 测试用例
void run_event_loop_benchmark() {
  PerformanceBenchmark benchmark;
  OptimizedEventLoop loop;

  auto result = benchmark.benchmark("EventLoopPost", [&]() {
    loop.post([]() {});
  }, 100000);

  std::cout << "Event Loop Post Performance:" << std::endl;
  std::cout << "  Operations per second: " << result.operations_per_second << std::endl;
  std::cout << "  Average latency (ms): " << result.avg_latency_ms << std::endl;
  std::cout << "  Maximum latency (ms): " << result.max_latency_ms << std::endl;
}
```

## 7. 预期改进

| 优化领域 | 预期性能提升 | 内存使用改进 | 可靠性提升 |
|---------|-------------|-------------|-------------|
| 事件循环 | 5-10倍 | - | 显著 |
| 日志系统 | 3-5倍 | 减少30-50% | 显著 |
| 指标系统 | 2-4倍 | 减少20-30% | 中等 |
| 内存管理 | 2-3倍 | 减少40-60% | 显著 |

## 8. 总结

通过对基础架构的全面优化，VeloZ 量化交易框架的性能、可靠性和可扩展性将得到显著提升。这些优化将使框架能够处理更高频率的交易信号，更低延迟的市场数据，以及更大规模的策略组合，为构建工业级量化交易系统奠定坚实基础。

优化过程中需要保持对现有功能的兼容性，确保所有测试通过，并提供详细的性能基准测试结果，以便验证优化效果和指导进一步的改进。
