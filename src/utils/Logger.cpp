#include "utils/Logger.h"
#include <chrono>
#include <thread>
#include <stdarg.h>
#include <string>

std::atomic<LogLevel> AsyncLogger::current_level_{DEBUG};

void AsyncLogger::setLevel(LogLevel level) {
    current_level_.store(level, std::memory_order_relaxed);
}

LogLevel AsyncLogger::getLevel() {
    return current_level_.load(std::memory_order_relaxed);
}

AsyncLogger& AsyncLogger::getInstance() {
    static AsyncLogger instance;
    return instance;
}

AsyncLogger::AsyncLogger() :
    is_running_(false),
    flush_interval_(3000),
    buffer_size_(8 * 1024 * 1024),
    file_size_(0) {
    current_buffer_ = std::make_unique<Buffer>();
    next_buffer_ = std::make_unique<Buffer>();
    current_buffer_->reserve(buffer_size_);
    next_buffer_->reserve(buffer_size_);
}

AsyncLogger::~AsyncLogger() {
    shutdown();
}

// 初始化
void AsyncLogger::init(const std::string& log_path, const std::string& file_name, size_t buff_size, int flush_interval) {
    std::lock_guard<std::mutex> lock(file_mtx_);
    log_path_ = log_path;
    file_name_ = file_name;
    buffer_size_ = buff_size;
    flush_interval_ = flush_interval;

    if (!fs::exists(log_path_)) {
        fs::create_directories(log_path_);
    }

    rollFile(); // 打开日志文件

    is_running_ = true;
    if (write_thread_ == nullptr) {
        write_thread_ = std::make_unique<std::thread>(&AsyncLogger::writeThreadFunc, this);
    }
}

const char* AsyncLogger::level2Str(LogLevel level) {
    switch (level) {
        case DEBUG: return "DEBUG";
        case INFO:  return "INFO";
        case WARN:  return "WARN";
        case ERROR: return "ERROR";
        case FATAL: return "FATAL";
        default:    return "UNKNOWN";
    }
}

void AsyncLogger::rollFile() {
    fs::path current = fs::path(log_path_) / file_name_;
    
    // 如果文件已打开，先关闭
    if (log_file_.is_open()) {
        log_file_.close();
    }
    
    // 如果当前日志文件存在，则重命名归档
    if (fs::exists(current)) {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::tm tm_buf;
        localtime_r(&time_t_now, &tm_buf);
        char time_str[32];
        std::strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", &tm_buf);
        fs::path archive = current;
        archive += "." + std::string(time_str) + "." + std::to_string(ms.count()) + ".log";
        fs::rename(current, archive);
    }
    
    // 重新打开当前日志文件
    log_file_.open(current, std::ios::app);
    file_size_ = 0;
    if (!log_file_.is_open()) {
        std::cerr << "AsyncLogger: rollFile failed to open " << current << std::endl;
    }
}

void AsyncLogger::log(LogLevel level, const char* file, int line, const char* format, ...) {
    if (!is_running_) {
        return;   // 确保关闭日志系统后不会再有日志写入
    }

    if (level < current_level_.load(std::memory_order_relaxed)) {
        return;
    }
    // 获取时间
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm_buf;
    localtime_r(&time_t_now, &tm_buf); // POSIX
    char time_str[32];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);
    
    // 消息体
    char msg_buf[1024];
    va_list ap;
    va_start(ap, format);
    int msg_len = vsnprintf(msg_buf, sizeof(msg_buf), format, ap); // 返回格式化后字符串的长度
    va_end(ap);
    if (msg_len < 0) { // 格式化出错
        msg_len = 0;
    }
    else if (msg_len >= (int)sizeof(msg_buf)) {
        msg_len = sizeof(msg_buf) - 1; // 减去'\0'
    }

    // 日志头（时间+等级+文件行号+线程id）
    char header[256];
    int header_len = snprintf(header, sizeof(header), 
        "[%s.%03d][%s][%s:%d][%lu] ",
        time_str,
        (int)ms.count(),
        level2Str(level),
        file,
        line,
        std::hash<std::thread::id>{}(std::this_thread::get_id())
    );

    std::lock_guard<std::mutex> lck(mtx_);
    size_t need_len = header_len + msg_len + 1; // +1 是换行符
    if (need_len > buffer_size_) {  // 单条日志过大需要截断
        msg_len = buffer_size_ - header_len - 1;
        if (msg_len < 0) {
            msg_len = 0;
        }
        need_len = buffer_size_;

    }
    if (current_buffer_->size() + need_len > buffer_size_) {
        if (next_buffer_->empty()) {
            current_buffer_.swap(next_buffer_);
            cv_.notify_one();
        } else {
            dropped_count_.fetch_add(1, std::memory_order_relaxed);
            if (dropped_count_ % 10000 == 0) {
                std::cerr << "AsyncLogger: dropped " << dropped_count_ << " logs (buffer full)" << std::endl;
            }
            return;
        }
    }

    current_buffer_->insert(current_buffer_->end(), header, header + header_len);
    current_buffer_->insert(current_buffer_->end(), msg_buf, msg_buf + msg_len);
    current_buffer_->push_back('\n');
}


void AsyncLogger::writeThreadFunc() {
    Buffer write_buffer;
    write_buffer.reserve(buffer_size_);
    while (is_running_) {
        {
            std::unique_lock<std::mutex> lck(mtx_);
            // 等待next_buffer_有数据
            cv_.wait_for(lck, std::chrono::milliseconds(flush_interval_), [this] {
                return !next_buffer_->empty() || !current_buffer_->empty() || !is_running_;
            });

            if (!next_buffer_->empty()) {
                write_buffer.swap(*next_buffer_);
            }
             else if (!current_buffer_->empty()) {  // 走到这个分支时next_buffer_一定为空
                // 超时且 current_buffer_ 有数据：交换到 next_buffer_ 再取走
                current_buffer_.swap(next_buffer_);
                write_buffer.swap(*next_buffer_);
            }
        }

        if (!write_buffer.empty()) {
            {
                std::lock_guard<std::mutex> lck(file_mtx_);
                if (file_size_ + write_buffer.size() >= MAX_FILE_SIZE) {
                    rollFile();
                }
                if (log_file_.is_open()) {
                    log_file_.write(write_buffer.data(), write_buffer.size());
                    log_file_.flush();
                    file_size_ += write_buffer.size();
                } else {
                    std::cerr.write(write_buffer.data(), write_buffer.size());
                    std::cerr << std::endl;
                }
            }
            write_buffer.clear();
        }
    }
    writeWhenExit();
}

void AsyncLogger::writeWhenExit() { 
    Buffer final_buf;
    final_buf.reserve(buffer_size_ * 2);
    {
        std::lock_guard<std::mutex> lck(mtx_);
        if (!next_buffer_->empty()) {
            final_buf.insert(final_buf.end(), next_buffer_->begin(), next_buffer_->end());
            next_buffer_->clear();
        }
    
        if (!current_buffer_->empty()) {
            final_buf.insert(final_buf.end(), current_buffer_->begin(), current_buffer_->end());
            current_buffer_->clear();
        }
    } // 避免持锁写

    if (!final_buf.empty()) {
        std::lock_guard<std::mutex> lck(file_mtx_);
        if (file_size_ + final_buf.size() >= MAX_FILE_SIZE) {
            rollFile();
        }
        if (log_file_.is_open()) {
            log_file_.write(final_buf.data(), final_buf.size());
            log_file_.flush();
        }
    }

}

void AsyncLogger::shutdown() {
    is_running_ = false;
    cv_.notify_all();
    if (write_thread_ && write_thread_->joinable()) {
        write_thread_->join();
    }
}
