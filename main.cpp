#include <iostream>
#include <threadpool/ThreadPool.h>
#include <chrono>

int main() {
#ifdef DEBUG
    std::cout << "Debug模式开启" << std::endl;
#endif
#ifdef RELEASE
    std::cout << "Release模式开启" << std::endl;
#endif
    thread_pool::ThreadPool pool(thread_pool::ThreadPool::Config{5});
    auto task = [](int a, int b) {
        std::cout << "线程id: " << std::this_thread::get_id() << "正在工作task(" << a << ", " << b << ")" << std::endl;
    };

    for (int i = 0; i < 100; i++) {
        pool.Submit(task, i, i + 3);
    }

    return 0;
}