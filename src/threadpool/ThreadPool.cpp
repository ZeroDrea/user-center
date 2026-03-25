#include <threadpool/ThreadPool.h>
#include <iostream>
namespace thread_pool {
ThreadPool::ThreadPool(const Config& config) 
    : config_(config) {
    size_t thread_count = config_.thread_count;
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) {
            thread_count = 1;
        }
    }

    config_.thread_count = thread_count;
    std::cout << "thread_count: " << thread_count << ", max_queue_size: " << config_.max_queue_size << std::endl;
    workers_.reserve(thread_count);
    for (size_t i = 0; i < thread_count; i++) {
        workers_.emplace_back(&ThreadPool::WorkerLoop, this);
    }
}

void ThreadPool::WorkerLoop() noexcept {
    std::cout << "工作线程id: " << std::this_thread::get_id() << "创建成功" << std::endl;
    while (true) {
        std::optional<Task> task;
        {
            std::unique_lock<std::mutex> lck(mutex_);
            cv_.wait(lck, [this] {
                return stop_ || !tasks_.empty();
            });

            if (stop_ && tasks_.empty()) {
                return;
            }

            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
            }
        }

        if (task.has_value()) {
            ExecuteTask(std::move(task.value()));
        }
    }
}

void ThreadPool::Cleanup() noexcept {
    int cnt = 0;
    {
        std::lock_guard<std::mutex> lck(mutex_);
        stop_ = true;
        std::queue<Task> empty;
        cnt = tasks_.size();
        swap(tasks_, empty);

        cv_.notify_all();
    }

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        } else {
            std::cout << "工作线程id: " << worker.get_id() << "回收失败" << std::endl;
        }

    }
    std::cout << "还有cnt: " << cnt << "任务没有完成" << std::endl;

}

void ThreadPool::ExecuteTask(Task&& task) noexcept {
    try {
        if (task.policy == LaunchPolicy::kDeferred) {
            // 延迟执行策略：在等待 future 时才执行
            // 这里直接执行，实际上由 submit 中的 packaged_task 控制
            task.func();
        } else {
            // 异步执行策略：立即执行
            task.func();
        }
    } catch (const std::exception& e) {

        // 记录异常但不传播，避免工作线程崩溃
        // 添加日志
        std::cerr << "Task execution failed: " << e.what() << std::endl;
    } catch (...) {
        // 捕获所有异常，确保工作线程不会崩溃
        std::cerr << "Task execution failed with unknown exception" << std::endl;
    }
}

ThreadPool::~ThreadPool() {
    Cleanup();
}

}