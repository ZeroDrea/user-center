#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <filesystem>
#include <memory>
#include <atomic>

// C++17 命名空间
namespace fs = std::filesystem;

// 日志等级
enum LogLevel {
    DEBUG = 0,
    INFO,
    WARN,
    ERROR,
    FATAL
};

// 异步日志类（单例模式）
class AsyncLogger {
public:
    // 禁止拷贝和移动
    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;
    AsyncLogger(AsyncLogger&&) = delete;
    AsyncLogger& operator=(AsyncLogger&&) = delete;

    // 获取单例
    static AsyncLogger& getInstance();

    static void setLevel(LogLevel level);  // 设置日志级别
    static LogLevel getLevel();     // 获取日志级别

    // 初始化日志：日志路径、文件名前缀、缓冲大小、刷新间隔(ms)
    void init(const std::string& log_path, const std::string& file_name,
              size_t buffer_size = 1024 * 1024 * 8, // 8M缓冲
              int flush_interval = 3000);           // 3秒自动刷新

    // 写日志接口
    void log(LogLevel level, const char* file, int line, const char* format, ...);

    // 关闭日志
    void shutdown();

private:
    // 私有构造（单例）
    AsyncLogger();
    ~AsyncLogger();

    // 后台写文件线程函数
    void writeThreadFunc();
    void writeWhenExit();  // 退出前写剩余数据

    // 等级转字符串
    const char* level2Str(LogLevel level);

    // 滚动日志文件（按大小/日期），调用之前必须持有file_mtx_锁
    void rollFile();

private:
    static std::atomic<LogLevel> current_level_;
    std::atomic<uint64_t> dropped_count_{0};     // 统计被丢弃的日志数目
    // 双缓冲类型：vector<char> 高效拼接字符串
    using Buffer = std::vector<char>;

    // 线程相关
    std::unique_ptr<std::thread> write_thread_;  // 后台写线程
    std::mutex mtx_;                             // 互斥锁，锁双缓冲区
    std::mutex file_mtx_;                        // 互斥锁，锁写入文件
    std::condition_variable cv_;                 // 条件变量
    std::atomic<bool> is_running_;        // 日志运行状态
    int flush_interval_;

    // 双缓冲核心
    std::unique_ptr<Buffer> current_buffer_;     // 当前写入缓冲
    std::unique_ptr<Buffer> next_buffer_;        // 备用缓冲
    size_t buffer_size_;                          // 单个缓冲大小

    // 文件相关
    std::ofstream log_file_;                      // 日志文件
    std::string log_path_;                        // 日志路径
    std::string file_name_;                       // 文件名前缀
    size_t file_size_;                            // 当前文件大小
    const size_t MAX_FILE_SIZE = 1024 * 1024 * 100; // 单文件最大100M
};

#define LOG_DEBUG(fmt, ...)  AsyncLogger::getInstance().log(DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)   AsyncLogger::getInstance().log(INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)   AsyncLogger::getInstance().log(WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)  AsyncLogger::getInstance().log(ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...)  AsyncLogger::getInstance().log(FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif // LOGGER_H