#include <iostream>
#include <threadpool/ThreadPool.h>
#include <chrono>
#include <stdarg.h>
#include <vector>
#include "utils/Logger.h"

int main() {
#ifdef DEBUG
    std::cout << "Debug模式开启" << std::endl;
#endif
#ifdef RELEASE
    std::cout << "Release模式开启" << std::endl;
#endif
    AsyncLogger::getInstance().init("./logs", "server");
 
    thread_pool::ThreadPool t(thread_pool::ThreadPool::Config(6));
    AsyncLogger::setLevel(LogLevel::INFO);
    for (size_t i = 0; i < 100000; i++) {
        t.Submit([=](){
            if (i % 2 == 0) {
                LOG_DEBUG("用来测试DEBUG日志 %d", i);
            } else if (i % 3 == 0) {
                LOG_INFO("用来测试INFO日志 %d", i);
            } else if (i % 4 == 0) {
                LOG_WARN("用来测试WARN日志 %d", i);
            } else if (i % 5 == 0) {
                LOG_ERROR("用来测试ERROR日志 %d", i);
            } else if (i % 7 == 0) {
                LOG_FATAL("用来测试FATAL日志 %d", i);
            }

            // if (i == 50000) {
            //     AsyncLogger::getInstance().shutdown();
            // }
        });
    }

    while (true) {
        std::this_thread::sleep_for(std::chrono::microseconds(5000));
    }

    return 0;
}
