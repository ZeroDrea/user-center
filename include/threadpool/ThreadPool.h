#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <vector>
#include <future>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <type_traits>
#include <atomic>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <iterator>
#include "utils/Logger.h"

namespace thread_pool {
/**
 * @brief 线程池异常
*/
class ThreadPoolException : public std::runtime_error {
public:
    explicit ThreadPoolException(const std::string& message) : std::runtime_error(message) {}
};

/**
 * @brief 线程池类
 * 线程安全，适用于高并发场景
 */
class ThreadPool {
public:
    struct Config {
        size_t thread_count;
        size_t max_queue_size;
        Config() 
            : thread_count(0)
            , max_queue_size(0) {}
        explicit Config(size_t threads) 
            : thread_count(threads)
            , max_queue_size(0) {}
        Config(size_t threads, size_t max_queue) 
            : thread_count(threads)
            , max_queue_size(max_queue) {}
    };
    ThreadPool() : ThreadPool(Config()) {}
    explicit ThreadPool(const Config& config);
    ~ThreadPool();

    // 禁止拷贝和移动
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * @brief 提交任务到线程池
     */

    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result_t<F, Args...>>;
    
private:
    using Task = std::function<void()>;
    void WorkerLoop();
    void Cleanup() noexcept;

private:
    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;

    std::mutex mutex_;
    std::condition_variable cv_;

    std::atomic<bool> stop_{false};

    Config config_;
};

// 模板实现
template<typename F, typename... Args>
auto ThreadPool::Submit(F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result_t<F, Args...>> {
    using ReturnType = typename std::invoke_result_t<F, Args...>;

    // 将参数打包到 tuple 中，以便在 lambda 中展开
    auto params = std::make_tuple(std::forward<Args>(args)...);
    
    // 创建 packaged_task，内部 lambda 负责调用用户函数并捕获异常
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        [func = std::forward<F>(f), params = std::move(params)]() mutable -> ReturnType {
            try {
                // 使用 std::apply 展开 tuple 调用函数
                return std::apply(func, std::move(params));
            } catch (const std::exception& e) {
                LOG_ERROR("Task threw exception: %s", e.what());
                throw;  // 重新抛出，让 packaged_task 捕获并存储到 future
            } catch (...) {
                LOG_ERROR("Task threw unknown exception");
                throw;
            }
        }
    );
    
    auto res = task->get_future();

    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_) {
        throw ThreadPoolException("ThreadPool is stopped");
    }
    if (config_.max_queue_size > 0 && tasks_.size() >= config_.max_queue_size) {
        throw ThreadPoolException("Task queue is full");
    }

    // 将 packaged_task 包装成无参函数放入队列
    tasks_.emplace([task]() { (*task)(); });
    cv_.notify_one();
    return res;
}

#if 0
template<typename F, typename... Args>
auto ThreadPool::Submit(F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result_t<F, Args...>> {
    using ReturnType = typename std::invoke_result_t<F, Args...>;

    auto packaged_task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    auto res = packaged_task->get_future();

    std::lock_guard<std::mutex> lck(mutex_);
    if (stop_) {
        throw ThreadPoolException("ThreadPool is stopped");
    }
    if (config_.max_queue_size > 0 && tasks_.size() >= config_.max_queue_size) {
        throw ThreadPoolException("Task queue is full");
    }

    // 直接创建 Task（std::function<void()>）
    tasks_.emplace([packaged_task]() { (*packaged_task)(); });
    cv_.notify_one();
    return res;
}
#endif
} // namespace thread_pool


#endif // THREAD_POOL_H