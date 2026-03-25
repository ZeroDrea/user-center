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
        // if (a == 35) {
        //     std::cout << "error 线程id: " << std::this_thread::get_id() << "正在工作task(" << a << ", " << b << ")" << std::endl;
        //     throw "出错了";
        // }
        std::cout << "线程id: " << std::this_thread::get_id() << "正在工作task(" << a << ", " << b << ")" << std::endl;
    };

    for (int i = 0; i < 100; i++) {
        auto f = pool.Submit(task, i, i + 3);
        // f.get();
    }

    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "------------------------------------" << std::endl;

    return 0;
}