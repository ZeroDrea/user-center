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
 
    // AsyncLogger::getInstance().shutdown();
    thread_pool::ThreadPool t(thread_pool::ThreadPool::Config(6));
    
    for (size_t i = 0; i < 100000000; i++) {
        t.Submit([=](){
            LOG_DEBUG("begin working begin working %d", i);
        });
    }

    while (true) {
        std::this_thread::sleep_for(std::chrono::microseconds(5000));
    }

    return 0;
}
