#include "utils/Logger.h"
#include <chrono>
#include <thread>
#include <stdarg.h>

void AsyncLogger::log(LogLevel level, const char* file, int line, const char* format, ...) {
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
    else if (msg_len >= sizeof(msg_buf)) {
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
    if (current_buffer_->size() + need_len > buffer_size_) {

        if (next_buffer_->empty()) {
            current_buffer_.swap(next_buffer_);
            cv_.notify_one();
        } else {
            // TODO：两个缓冲区都满了，该怎么做
            return; // 丢弃？？
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
            cv_.wait_for(lck, std::chrono::milliseconds(3000), [this] {
                return !next_buffer_->empty() || !is_running_;
            });

            if (!next_buffer_->empty()) {
                write_buffer.swap(*next_buffer_);
            }
        }

        if (!write_buffer.empty()) {
            bool need_roll = false;
            need_roll = write_buffer.size() + file_size_ >= MAX_FILE_SIZE;  
            if (need_roll) {
                rollFile();
            }

            if (log_file_.is_open()) {
                log_file_.write(write_buffer.data(), write_buffer.size());
                log_file_.flush();
                file_size_ += write_buffer.size();
            } else {
                // 降级输出到 stderr
                std::cerr.write(write_buffer.data(), write_buffer.size());
                std::cerr << std::endl;
            }
            write_buffer.clear();
        }
    }

    // 退出时刷盘，要获取双缓冲的锁，因为此时可能存在其他线程写日志
    std::lock_guard<std::mutex> lck(mtx_);
    if (!next_buffer_->empty()) {
        if (log_file_.is_open()) {
            log_file_.write(next_buffer_->data(), next_buffer_->size());
        }
        next_buffer_->clear();
    }
    if (!current_buffer_->empty()) {
        if (log_file_.is_open()) {
            log_file_.write(current_buffer_->data(), current_buffer_->size());
        }
    }
    if (log_file_.is_open()) {
        log_file_.flush();
    }
}