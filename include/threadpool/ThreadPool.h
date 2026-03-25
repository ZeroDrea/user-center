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

namespace thread_pool {
/**
 * @brief 线程池异常
*/
class ThreadPoolException : public std::runtime_error {
public:
    explicit ThreadPoolException(const std::string& message) : std::runtime_error(message) {}
};

/**
 * @brief 任务执行策略
 */
enum class LaunchPolicy {
    kAsync,     // 异步执行（默认）
    kDeferred   // 延迟执行（暂未实现，保留扩展）
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
        LaunchPolicy default_policy;  // 默认执行策略
        Config() 
            : thread_count(0)
            , max_queue_size(0)
            , default_policy(LaunchPolicy::kAsync) {}
        explicit Config(size_t threads) 
            : thread_count(threads)
            , max_queue_size(0)
            , default_policy(LaunchPolicy::kAsync) {}
        Config(size_t threads, size_t max_queue) 
            : thread_count(threads)
            , max_queue_size(max_queue)
            , default_policy(LaunchPolicy::kAsync) {}
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

    template<typename F, typename... Args>
    auto Submit(LaunchPolicy policy, F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result_t<F, Args...>>;
    
    /**
     * @brief 批量提交任务
     */
    template<typename InputIt, typename F>
    auto SubmitBitch(InputIt first, InputIt last, F&& f) 
        -> std::vector<std::future<typename std::invoke_result_t<F,
            typename std::iterator_traits<InputIt>::value_type>>>;
    
private:
    struct Task {
        std::function<void()> func;
        LaunchPolicy policy;

        Task() = default;

        template<typename F>
        explicit Task(F&& f, LaunchPolicy p = LaunchPolicy::kAsync)
            : func(std::forward<F>(f)), policy(p) {}
    };

    void WorkerLoop();
    void ExecuteTask(Task&& task);
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
    return Submit(LaunchPolicy::kAsync, std::forward<F>(f), std::forward<Args>(args)...);
}

template<typename F, typename... Args>
auto ThreadPool::Submit(LaunchPolicy policy, F&& f, Args&&... args) 
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

    // 队列满判断
    if (config_.max_queue_size > 0 && tasks_.size() >= config_.max_queue_size) {
        throw ThreadPoolException("Task queue is full");
    }

    auto task_func = [packaged_task]() {
        (*packaged_task)();
    };
    tasks_.emplace(std::move(task_func), policy);
    cv_.notify_one();
    return res;
}

template<typename InputIt, typename F>
auto ThreadPool::SubmitBitch(InputIt first, InputIt last, F&& f) 
    -> std::vector<std::future<typename std::invoke_result_t<F, 
        typename std::iterator_traits<InputIt>::value_type>>> {
    using ValueType = typename std::iterator_traits<InputIt>::value_type;
    using ReturnType = typename std::invoke_result_t<F, ValueType>;

    std::vector<std::future<ReturnType>> futures;
    futures.reserve(static_cast<size_t>(std::distance(first, last)));

    for (auto it = first; it != last; ++it) {
        futures.push_back(Submit(std::forward<F>(f), *it));
    }

    return futures;
}

} // namespace thread_pool


#endif // THREAD_POOL_H