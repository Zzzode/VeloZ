# VeloZ Quantitative Trading Framework - Event Loop Performance Optimization Plan

## 1. Problem Analysis

The current event loop implementation uses traditional mutex locks and condition variables, which has the following performance issues:
1. Using mutex locks to protect the task queue results in lock contention issues under high concurrency
2. Using priority_queue to manage delayed tasks requires reordering on each operation
3. Using condition variables to wait for tasks results in higher wake-up and scheduling overhead
4. Task execution and scheduling are on the same thread, which may cause long-running tasks to block other tasks

To improve performance, we will implement an event loop based on io_uring, while optimizing task scheduling algorithms and timer management.

## 2. Optimization Plan

### 2.1 Upgrade to io_uring

io_uring is an asynchronous I/O interface introduced in Linux 5.1, offering higher performance than traditional epoll. We will use io_uring to replace the traditional epoll interface.

### 2.2 Optimize Task Scheduling

- Use lock-free queues instead of traditional mutex-protected queues
- Implement efficient task priority scheduling
- Optimize delayed task management, using min-heap instead of priority_queue

### 2.3 Efficient Timer Management

- Use hierarchical time wheel algorithm to manage timers
- Implement efficient timer lookup, addition, and deletion operations
- Reduce time complexity of timer management

### 2.4 Concurrent Event Distribution

- Implement separation of task execution and scheduling
- Use worker thread pools to execute tasks
- Optimize concurrent processing capability for event distribution

## 3. Implementation Code

### 3.1 Lock-Free Queue Implementation

```cpp
// Lock-free queue implementation
template <typename T>
class LockFreeQueue {
public:
    LockFreeQueue() {
        head_ = new Node;
        tail_ = head_;
    }

    ~LockFreeQueue() {
        while (head_) {
            auto next = head_->next;
            delete head_;
            head_ = next;
        }
    }

    // Enqueue operation
    void push(T value) {
        auto new_node = new Node(kj::mv(value));
        Node* old_tail = tail_.load();

        while (true) {
            Node* old_tail_next = old_tail->next.load();

            if (old_tail == tail_.load()) {
                if (!old_tail_next) {
                    if (old_tail->next.compare_exchange_weak(old_tail_next, new_node)) {
                        tail_.compare_exchange_strong(old_tail, new_node);
                        return;
                    }
                } else {
                    tail_.compare_exchange_strong(old_tail, old_tail_next);
                }
            } else {
                old_tail = tail_.load();
            }
        }
    }

    // Dequeue operation
    bool pop(T& value) {
        Node* old_head = head_.load();

        while (true) {
            Node* old_head_next = old_head->next.load();

            if (!old_head_next) {
                return false;
            }

            if (old_head == head_.load()) {
                if (head_.compare_exchange_weak(old_head, old_head_next)) {
                    value = kj::mv(old_head_next->data);
                    delete old_head;
                    return true;
                }
            } else {
                old_head = head_.load();
            }
        }
    }

    // Check if queue is empty
    bool empty() const {
        return !head_.load()->next.load();
    }

private:
    // Node structure
    struct Node {
        explicit Node(T data = T()) : data(kj::mv(data)) {}

        std::atomic<Node*> next{nullptr};
        T data;
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
};
```

### 3.2 Hierarchical Time Wheel Timer

```cpp
// Hierarchical time wheel timer implementation
class HierarchicalTimerWheel {
public:
    HierarchicalTimerWheel() : current_tick_(0), running_(false) {}

    ~HierarchicalTimerWheel() {
        stop();
    }

    // Start timer
    bool start() {
        if (running_) {
            return true;
        }

        running_ = true;
        timer_thread_ = std::thread([this]() {
            timerLoop();
        });

        return true;
    }

    // Stop timer
    bool stop() {
        if (!running_) {
            return true;
        }

        running_ = false;
        if (timer_thread_.joinable()) {
            timer_thread_.join();
        }

        return true;
    }

    // Add timer
    void addTimer(uint64_t delay_ms, std::function<void()> callback) {
        uint64_t expiration = current_tick_ + delay_ms;
        uint64_t bucket_index = expiration % BUCKET_COUNT;

        std::lock_guard<std::mutex> lock(buckets_mutex_[bucket_index]);
        buckets_[bucket_index].emplace_back(expiration, kj::mv(callback));
    }

    // Cancel timer (simplified implementation, actual implementation needs more complex logic)
    void cancelTimer() {
        // Actual implementation needs to track timer IDs and delete corresponding timers
    }

    // Get current tick
    uint64_t getCurrentTick() const {
        return current_tick_.load();
    }

private:
    static constexpr size_t BUCKET_COUNT = 60 * 1000; // 1 minute buckets
    static constexpr uint64_t TICK_INTERVAL_MS = 1; // 1ms per tick

    // Timer node
    struct TimerNode {
        uint64_t expiration;
        std::function<void()> callback;

        bool operator<(const TimerNode& other) const {
            return expiration < other.expiration;
        }
    };

    // Timer loop
    void timerLoop() {
        auto next_tick = std::chrono::steady_clock::now();

        while (running_) {
            next_tick += std::chrono::milliseconds(TICK_INTERVAL_MS);
            std::this_thread::sleep_until(next_tick);

            uint64_t current = current_tick_.fetch_add(1);
            uint64_t bucket_index = current % BUCKET_COUNT;

            std::vector<TimerNode> expired_timers;

            {
                std::lock_guard<std::mutex> lock(buckets_mutex_[bucket_index]);
                auto& bucket = buckets_[bucket_index];

                // Collect expired timers
                auto it = std::remove_if(bucket.begin(), bucket.end(), [current](const TimerNode& node) {
                    if (node.expiration <= current) {
                        return true;
                    }
                    return false;
                });

                expired_timers.insert(expired_timers.end(), it, bucket.end());
                bucket.erase(it, bucket.end());
            }

            // Execute callbacks for expired timers
            for (const auto& timer : expired_timers) {
                try {
                    timer.callback();
                } catch (const std::exception& e) {
                    LOG_ERROR("Timer callback error: " << e.what());
                }
            }
        }
    }

    std::atomic<uint64_t> current_tick_;
    std::vector<std::vector<TimerNode>> buckets_{BUCKET_COUNT};
    std::vector<std::mutex> buckets_mutex_{BUCKET_COUNT};
    std::thread timer_thread_;
    std::atomic<bool> running_;
};
```

### 3.3 io_uring-based Event Loop

```cpp
// io_uring-based event loop implementation
class IoUringEventLoop {
public:
    IoUringEventLoop() : running_(false), io_uring_fd_(-1) {}

    ~IoUringEventLoop() {
        stop();
    }

    // Initialize io_uring
    bool initialize() {
        if (io_uring_fd_ != -1) {
            return true;
        }

        io_uring_params params;
        memset(&params, 0, sizeof(params));

        io_uring_fd_ = io_uring_setup(32, &params);
        if (io_uring_fd_ < 0) {
            LOG_ERROR("Failed to setup io_uring: " << strerror(errno));
            return false;
        }

        // Allocate ring buffer
        ring_ = static_cast<struct io_uring*>(mmap(nullptr,
            params.sq_off.array + params.sq_entries * sizeof(uint32_t),
            PROT_READ | PROT_WRITE, MAP_SHARED, io_uring_fd_, IORING_OFF_SQ_RING));

        if (ring_ == MAP_FAILED) {
            LOG_ERROR("Failed to map io_uring ring: " << strerror(errno));
            close(io_uring_fd_);
            io_uring_fd_ = -1;
            return false;
        }

        return true;
    }

    // Start event loop
    bool start() {
        if (running_) {
            return true;
        }

        if (!initialize()) {
            return false;
        }

        running_ = true;
        event_loop_thread_ = std::thread([this]() {
            runLoop();
        });

        return true;
    }

    // Stop event loop
    bool stop() {
        if (!running_) {
            return true;
        }

        running_ = false;
        if (event_loop_thread_.joinable()) {
            event_loop_thread_.join();
        }

        if (ring_ != MAP_FAILED) {
            munmap(ring_, 0); // Actually need to calculate the correct size
        }

        if (io_uring_fd_ != -1) {
            close(io_uring_fd_);
        }

        return true;
    }

    // Submit task
    void post(std::function<void()> task) {
        task_queue_.push(kj::mv(task));
        wakeup();
    }

    // Submit delayed task
    void postDelayed(std::function<void()> task, std::chrono::milliseconds delay) {
        timer_.addTimer(delay.count(), [this, task = kj::mv(task)]() {
            post(kj::mv(task));
        });
    }

    // Get number of pending tasks
    size_t pendingTasks() const {
        return task_queue_.size();
    }

private:
    // Event loop main function
    void runLoop() {
        while (running_) {
            // Process tasks
            processTasks();

            // Wait for events
            waitForEvents();
        }

        // Process remaining tasks
        processTasks();
    }

    // Process tasks
    void processTasks() {
        std::function<void()> task;

        while (task_queue_.pop(task)) {
            try {
                task();
            } catch (const std::exception& e) {
                LOG_ERROR("Task execution error: " << e.what());
            }
        }
    }

    // Wait for events
    void waitForEvents() {
        struct io_uring_cqe* cqe;
        int ret = io_uring_wait_cqe(io_uring_fd_, &cqe);

        if (ret < 0) {
            if (errno == EINTR) {
                return;
            }
            LOG_ERROR("io_uring_wait_cqe failed: " << strerror(errno));
            return;
        }

        // Process events
        processCompletionQueueEntry(cqe);

        io_uring_cqe_seen(io_uring_fd_, cqe);
    }

    // Process completion queue entry
    void processCompletionQueueEntry(struct io_uring_cqe* cqe) {
        if (cqe->res < 0) {
            LOG_ERROR("Operation failed: " << strerror(-cqe->res));
            return;
        }

        // Process tasks after event completion
        // Here you can call corresponding callback functions based on cqe->user_data
    }

    // Wake up event loop
    void wakeup() {
        // Use io_uring's NOOP operation to wake up the event loop
        struct io_uring_sqe* sqe = io_uring_get_sqe(io_uring_fd_);
        if (!sqe) {
            LOG_ERROR("Failed to get sqe");
            return;
        }

        io_uring_prep_nop(sqe);
        sqe->user_data = 0;

        int ret = io_uring_submit(io_uring_fd_);
        if (ret < 0) {
            LOG_ERROR("io_uring_submit failed: " << strerror(errno));
        }
    }

    std::atomic<bool> running_;
    LockFreeQueue<std::function<void()>> task_queue_;
    HierarchicalTimerWheel timer_;
    std::thread event_loop_thread_;
    int io_uring_fd_;
    struct io_uring* ring_;
};
```

### 3.4 Worker Thread Pool

```cpp
// Worker thread pool implementation
class WorkerThreadPool {
public:
    explicit WorkerThreadPool(size_t num_threads = std::thread::hardware_concurrency())
        : num_threads_(num_threads), running_(false) {}

    ~WorkerThreadPool() {
        stop();
    }

    // Start thread pool
    bool start() {
        if (running_) {
            return true;
        }

        running_ = true;
        workers_.reserve(num_threads_);

        for (size_t i = 0; i < num_threads_; ++i) {
            workers_.emplace_back([this, i]() {
                workerLoop(i);
            });
        }

        return true;
    }

    // Stop thread pool
    bool stop() {
        if (!running_) {
            return true;
        }

        running_ = false;
        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        workers_.clear();
        return true;
    }

    // Submit task
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
        using ReturnType = typename std::result_of<F(Args...)>::type;
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_.emplace_back([task]() { (*task)(); });
        }

        condition_.notify_one();

        return result;
    }

    // Get thread count
    size_t threadCount() const {
        return num_threads_;
    }

    // Get number of pending tasks
    size_t pendingTasks() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return tasks_.size();
    }

private:
    // Worker thread loop
    void workerLoop(size_t thread_id) {
        while (running_) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(mutex_);

                condition_.wait(lock, [this]() {
                    return !tasks_.empty() || !running_;
                });

                if (!running_ && tasks_.empty()) {
                    break;
                }

                if (!tasks_.empty()) {
                    task = kj::mv(tasks_.front());
                    tasks_.pop_front();
                }
            }

            if (task) {
                try {
                    task();
                } catch (const std::exception& e) {
                    LOG_ERROR("Worker thread " << thread_id << " task execution error: " << e.what());
                }
            }
        }
    }

    size_t num_threads_;
    std::atomic<bool> running_;
    std::vector<std::thread> workers_;
    std::deque<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
};
```

## 4. Integration Testing

### 4.1 Performance Comparison Testing

```cpp
// Performance comparison test
void runPerformanceComparison() {
    PerformanceTester tester;

    // Test traditional event loop
    EventLoop traditional_loop;

    auto traditional_result = tester.test("TraditionalEventLoop", 10000, [&]() {
        static int counter = 0;
        traditional_loop.post([counter]() {
            volatile int result = counter * 2;
        });
    });

    tester.printResults(traditional_result);

    // Test optimized event loop
    IoUringEventLoop optimized_loop;
    optimized_loop.initialize();
    optimized_loop.start();

    auto optimized_result = tester.test("OptimizedEventLoop", 10000, [&]() {
        static int counter = 0;
        optimized_loop.post([counter]() {
            volatile int result = counter * 2;
        });
    });

    tester.printResults(optimized_result);

    optimized_loop.stop();

    // Test worker thread pool
    WorkerThreadPool thread_pool;
    thread_pool.start();

    auto thread_pool_result = tester.test("WorkerThreadPool", 10000, [&]() {
        static int counter = 0;
        thread_pool.submit([counter]() {
            volatile int result = counter * 2;
        });
    });

    tester.printResults(thread_pool_result);

    thread_pool.stop();
}
```

### 4.2 Stress Testing

```cpp
// Stress test
void runStressTest() {
    PerformanceTester tester;
    IoUringEventLoop loop;
    loop.initialize();
    loop.start();

    // Test concurrent task submission
    std::vector<std::thread> test_threads;
    const int thread_count = 10;
    const int tasks_per_thread = 10000;

    auto test_task = [&loop](int thread_id) {
        for (int i = 0; i < tasks_per_thread; ++i) {
            loop.post([thread_id, i]() {
                volatile int result = thread_id * 10000 + i;
            });
        }
    };

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < thread_count; ++i) {
        test_threads.emplace_back(test_task, i);
    }

    for (auto& thread : test_threads) {
        thread.join();
    }

    // Wait for all tasks to complete
    while (loop.pendingTasks() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    std::cout << "Stress test completed:\n";
    std::cout << "Threads: " << thread_count << "\n";
    std::cout << "Tasks per thread: " << tasks_per_thread << "\n";
    std::cout << "Total tasks: " << thread_count * tasks_per_thread << "\n";
    std::cout << "Duration: " << duration_ms << " ms\n";
    std::cout << "Operations per second: " << (thread_count * tasks_per_thread * 1000.0 / duration_ms) << "\n";

    loop.stop();
}
```

## 5. Optimization Effect Evaluation

### 5.1 Expected Performance Improvement

| Optimization Item | Expected Performance Improvement |
|-----------------|--------------------------------|
| Lock-free queue | 2-3x |
| Hierarchical time wheel | 3-4x |
| io_uring | 10-20x |
| Worker thread pool | 5-10x |

### 5.2 Test Result Analysis

In actual testing, we need to focus on the following metrics:
1. Latency of task submission and execution
2. Throughput of the event loop
3. Memory usage
4. CPU utilization
5. Performance under high concurrency

## 6. Implementation Plan

### 6.1 Phase 1 (2 weeks)

1. Complete implementation and testing of lock-free queue
2. Complete implementation and testing of hierarchical time wheel timer
3. Complete implementation and testing of worker thread pool

### 6.2 Phase 2 (3 weeks)

1. Complete implementation of io_uring-based event loop
2. Integrate all components
3. Conduct functional testing and performance testing

### 6.3 Phase 3 (2 weeks)

1. Conduct stress testing and load testing
2. Optimize and fix discovered issues
3. Complete documentation and example code

## 7. Risk Assessment

### 7.1 Technical Risks

1. **io_uring Compatibility**: io_uring is an interface introduced in Linux 5.1, need to ensure the target system supports it
2. **Performance Instability**: io_uring may not perform as expected in some situations
3. **Memory Management**: Memory management of lock-free data structures requires careful handling

### 7.2 Implementation Risks

1. **Development Complexity**: io_uring interface is relatively complex, requiring more development time
2. **Debugging Difficulty**: Debugging lock-free data structures and concurrent code is more difficult
3. **Test Coverage**: Need to cover various edge cases and concurrent scenarios

### 7.3 Mitigation Measures

1. **Compatibility Check**: Check if the system supports io_uring during initialization
2. **Fallback Strategy**: Use traditional epoll implementation on systems that don't support io_uring
3. **Comprehensive Testing**: Increase test coverage, including unit tests, integration tests, and stress tests

## 8. Summary

Through this optimization, we will upgrade the event loop's performance to industrial standards, capable of supporting high-concurrency, low-latency trading systems. Using io_uring to replace the traditional epoll interface, combined with lock-free queues, hierarchical time wheel timers, and worker thread pools, we can achieve the following improvements:
1. Significantly reduce latency of task submission and execution
2. Improve event loop throughput
3. Better utilize system resources
4. Support greater concurrent processing capabilities

These optimization measures will lay a solid foundation for the live trading system of the VeloZ quantitative trading framework.
