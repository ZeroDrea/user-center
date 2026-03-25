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
        size_t thread_count = 0;
        size_t max_queue_size = 0;
        Config() = default;
        explicit Config(size_t threads) : thread_count(threads) {}
        Config(size_t threads, size_t max_queue) : thread_count(threads), max_queue_size(max_queue) {}
    };
};

} // namespace thread_pool


#endif // THREAD_POOL_H