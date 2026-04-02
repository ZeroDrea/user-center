#ifndef LOGGER_H
#define LOGGER_H
#include <vector>
#include <memory>
// 日志等级
enum LogLevel {
    DEBUG = 0,
    INFO,
    WARN,
    ERROR,
    FATAL
};

// 异步日志类（单例模式）
class Logger {
public:
    // 禁止拷贝和移动
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    // 获取单例
    static Logger& getInstance();

    // 写日志接口（可变参数，printf风格）
    void log(LogLevel level, const char* file, int line, const char* format, ...);

private:
    // 私有构造（单例）
    Logger();
    ~Logger();
    
private:
    std::unique_ptr<std::vector<char>> buffer_;     // 写入缓冲

};

// 对外易用宏（业务代码直接使用）
#define LOG_DEBUG(...)  Logger::getInstance().log(DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)   Logger::getInstance().log(INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)   Logger::getInstance().log(WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)  Logger::getInstance().log(ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...)  Logger::getInstance().log(FATAL, __FILE__, __LINE__, __VA_ARGS__)

#endif // LOGGER_H