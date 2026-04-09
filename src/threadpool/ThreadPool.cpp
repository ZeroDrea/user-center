#include <iostream>
#include "threadpool/ThreadPool.h"
#include "utils/Logger.h"

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
    LOG_INFO("create thread pool success, thread_count(%zu), max_queue_size(%zu).", 
        thread_count, config_.max_queue_size);
    workers_.reserve(thread_count);
    for (size_t i = 0; i < thread_count; i++) {
        workers_.emplace_back(&ThreadPool::WorkerLoop, this);
    }
}

void ThreadPool::WorkerLoop() {
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

        if (task) {
            (*task)();
        }
    }
}

void ThreadPool::Cleanup() noexcept {
    // 停止接受新任务
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();

    // 等待所有工作线程退出（它们会消费完队列中的所有任务）
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    // 检查是否还有遗留任务
    size_t remaining = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        remaining = tasks_.size();
        if (remaining > 0) {
            LOG_WARN("ThreadPool destroyed with %zu pending tasks", remaining);
            tasks_ = {};  // 强制清空
        }
    }
}

ThreadPool::~ThreadPool() {
    Cleanup();
}

}