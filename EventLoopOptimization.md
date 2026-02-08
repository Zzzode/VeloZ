# VeloZ 量化交易框架 - 事件循环性能提升优化方案

## 1. 问题分析

当前事件循环实现使用传统的互斥锁和条件变量的方式，存在以下性能问题：
1. 使用互斥锁保护任务队列，在高并发情况下会有锁竞争问题
2. 使用 priority_queue 管理延迟任务，每次操作都需要重新排序
3. 使用条件变量等待任务，唤醒和调度开销较大
4. 任务执行和调度在同一线程，可能导致长时间任务阻塞其他任务

为了提升性能，我们将实现一个基于 io_uring 的事件循环，同时优化任务调度算法和定时器管理。

## 2. 优化方案

### 2.1 升级到 io_uring

io_uring 是 Linux 5.1 引入的异步 I/O 接口，相比传统的 epoll 有更高的性能。我们将使用 io_uring 替代传统的 epoll 接口。

### 2.2 优化任务调度

- 使用无锁队列替代传统的互斥锁保护的队列
- 实现高效的任务优先级调度
- 优化延迟任务管理，使用最小堆代替 priority_queue

### 2.3 高效的定时器管理

- 使用分层时间轮算法管理定时器
- 实现高效的定时器查找、添加和删除操作
- 减少定时器管理的时间复杂度

### 2.4 并发事件分发

- 实现任务执行和调度的分离
- 使用工作线程池执行任务
- 优化事件分发的并发处理能力

## 3. 实现代码

### 3.1 无锁队列实现

```cpp
// 无锁队列实现
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

    // 入队操作
    void push(T value) {
        auto new_node = new Node(std::move(value));
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

    // 出队操作
    bool pop(T& value) {
        Node* old_head = head_.load();

        while (true) {
            Node* old_head_next = old_head->next.load();

            if (!old_head_next) {
                return false;
            }

            if (old_head == head_.load()) {
                if (head_.compare_exchange_weak(old_head, old_head_next)) {
                    value = std::move(old_head_next->data);
                    delete old_head;
                    return true;
                }
            } else {
                old_head = head_.load();
            }
        }
    }

    // 检查队列是否为空
    bool empty() const {
        return !head_.load()->next.load();
    }

private:
    // 节点结构
    struct Node {
        explicit Node(T data = T()) : data(std::move(data)) {}

        std::atomic<Node*> next{nullptr};
        T data;
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
};
```

### 3.2 分层时间轮定时器

```cpp
// 分层时间轮定时器实现
class HierarchicalTimerWheel {
public:
    HierarchicalTimerWheel() : current_tick_(0), running_(false) {}

    ~HierarchicalTimerWheel() {
        stop();
    }

    // 启动定时器
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

    // 停止定时器
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

    // 添加定时器
    void addTimer(uint64_t delay_ms, std::function<void()> callback) {
        uint64_t expiration = current_tick_ + delay_ms;
        uint64_t bucket_index = expiration % BUCKET_COUNT;

        std::lock_guard<std::mutex> lock(buckets_mutex_[bucket_index]);
        buckets_[bucket_index].emplace_back(expiration, std::move(callback));
    }

    // 取消定时器（简化实现，实际需要更复杂的逻辑）
    void cancelTimer() {
        // 实际实现需要跟踪定时器ID并删除对应的定时器
    }

    // 获取当前 tick
    uint64_t getCurrentTick() const {
        return current_tick_.load();
    }

private:
    static constexpr size_t BUCKET_COUNT = 60 * 1000; // 1分钟的桶数
    static constexpr uint64_t TICK_INTERVAL_MS = 1; // 1ms per tick

    // 定时器节点
    struct TimerNode {
        uint64_t expiration;
        std::function<void()> callback;

        bool operator<(const TimerNode& other) const {
            return expiration < other.expiration;
        }
    };

    // 定时器循环
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

                // 收集过期定时器
                auto it = std::remove_if(bucket.begin(), bucket.end(), [current](const TimerNode& node) {
                    if (node.expiration <= current) {
                        return true;
                    }
                    return false;
                });

                expired_timers.insert(expired_timers.end(), it, bucket.end());
                bucket.erase(it, bucket.end());
            }

            // 执行过期定时器的回调
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

### 3.3 基于 io_uring 的事件循环

```cpp
// 基于 io_uring 的事件循环实现
class IoUringEventLoop {
public:
    IoUringEventLoop() : running_(false), io_uring_fd_(-1) {}

    ~IoUringEventLoop() {
        stop();
    }

    // 初始化 io_uring
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

        // 分配环形缓冲区
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

    // 启动事件循环
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

    // 停止事件循环
    bool stop() {
        if (!running_) {
            return true;
        }

        running_ = false;
        if (event_loop_thread_.joinable()) {
            event_loop_thread_.join();
        }

        if (ring_ != MAP_FAILED) {
            munmap(ring_, 0); // 实际需要计算正确的大小
        }

        if (io_uring_fd_ != -1) {
            close(io_uring_fd_);
        }

        return true;
    }

    // 提交任务
    void post(std::function<void()> task) {
        task_queue_.push(std::move(task));
        wakeup();
    }

    // 提交延迟任务
    void postDelayed(std::function<void()> task, std::chrono::milliseconds delay) {
        timer_.addTimer(delay.count(), [this, task = std::move(task)]() {
            post(std::move(task));
        });
    }

    // 获取待处理任务数量
    size_t pendingTasks() const {
        return task_queue_.size();
    }

private:
    // 事件循环主函数
    void runLoop() {
        while (running_) {
            // 处理任务
            processTasks();

            // 等待事件
            waitForEvents();
        }

        // 处理剩余任务
        processTasks();
    }

    // 处理任务
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

    // 等待事件
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

        // 处理事件
        processCompletionQueueEntry(cqe);

        io_uring_cqe_seen(io_uring_fd_, cqe);
    }

    // 处理完成队列条目
    void processCompletionQueueEntry(struct io_uring_cqe* cqe) {
        if (cqe->res < 0) {
            LOG_ERROR("Operation failed: " << strerror(-cqe->res));
            return;
        }

        // 处理事件完成后的任务
        // 这里可以根据 cqe->user_data 调用相应的回调函数
    }

    // 唤醒事件循环
    void wakeup() {
        // 使用 io_uring 的 NOOP 操作来唤醒事件循环
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

### 3.4 工作线程池

```cpp
// 工作线程池实现
class WorkerThreadPool {
public:
    explicit WorkerThreadPool(size_t num_threads = std::thread::hardware_concurrency())
        : num_threads_(num_threads), running_(false) {}

    ~WorkerThreadPool() {
        stop();
    }

    // 启动线程池
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

    // 停止线程池
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

    // 提交任务
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

    // 获取线程数量
    size_t threadCount() const {
        return num_threads_;
    }

    // 获取待处理任务数量
    size_t pendingTasks() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return tasks_.size();
    }

private:
    // 工作线程循环
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
                    task = std::move(tasks_.front());
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

## 4. 集成测试

### 4.1 性能对比测试

```cpp
// 性能对比测试
void runPerformanceComparison() {
    PerformanceTester tester;

    // 测试传统事件循环
    EventLoop traditional_loop;

    auto traditional_result = tester.test("TraditionalEventLoop", 10000, [&]() {
        static int counter = 0;
        traditional_loop.post([counter]() {
            volatile int result = counter * 2;
        });
    });

    tester.printResults(traditional_result);

    // 测试优化后的事件循环
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

    // 测试工作线程池
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

### 4.2 压力测试

```cpp
// 压力测试
void runStressTest() {
    PerformanceTester tester;
    IoUringEventLoop loop;
    loop.initialize();
    loop.start();

    // 测试并发任务提交
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

    // 等待所有任务完成
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

## 5. 优化效果评估

### 5.1 预期性能提升

| 优化项目 | 预期性能提升 |
|---------|-------------|
| 无锁队列 | 2-3倍 |
| 分层时间轮 | 3-4倍 |
| io_uring | 10-20倍 |
| 工作线程池 | 5-10倍 |

### 5.2 测试结果分析

在实际测试中，我们需要关注以下指标：
1. 任务提交和执行的延迟
2. 事件循环的吞吐量
3. 内存使用情况
4. CPU 利用率
5. 高并发下的性能表现

## 6. 实施计划

### 6.1 第一阶段（2周）

1. 完成无锁队列的实现和测试
2. 完成分层时间轮定时器的实现和测试
3. 完成工作线程池的实现和测试

### 6.2 第二阶段（3周）

1. 完成基于 io_uring 的事件循环实现
2. 集成所有组件
3. 进行功能测试和性能测试

### 6.3 第三阶段（2周）

1. 进行压力测试和负载测试
2. 优化和修复发现的问题
3. 完成文档和示例代码

## 7. 风险评估

### 7.1 技术风险

1. **io_uring 兼容性**：io_uring 是 Linux 5.1 引入的接口，需要确保目标系统支持
2. **性能不稳定**：io_uring 在某些情况下可能表现不如预期
3. **内存管理**：无锁数据结构的内存管理需要小心处理

### 7.2 实施风险

1. **开发复杂度**：io_uring 接口相对复杂，需要更多的开发时间
2. **调试难度**：无锁数据结构和并发代码的调试难度较大
3. **测试覆盖**：需要覆盖各种边缘情况和并发场景

### 7.3 缓解措施

1. **兼容性检查**：在初始化阶段检查系统是否支持 io_uring
2. **降级策略**：在不支持 io_uring 的系统上使用传统的 epoll 实现
3. **详细测试**：增加测试覆盖，包括单元测试、集成测试和压力测试

## 8. 总结

通过本次优化，我们将事件循环的性能提升到工业级标准，能够支持高并发、低延迟的交易系统。使用 io_uring 替代传统的 epoll 接口，结合无锁队列、分层时间轮定时器和工作线程池，我们可以实现以下改进：
1. 显著降低任务提交和执行的延迟
2. 提高事件循环的吞吐量
3. 更好地利用系统资源
4. 支持更大的并发处理能力

这些优化措施将为 VeloZ 量化交易框架的实盘交易系统奠定坚实的基础。
