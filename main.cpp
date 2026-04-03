#include <iostream>
#include <threadpool/ThreadPool.h>
#include <chrono>
#include <stdarg.h>
#include <vector>

void fargs(int x, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, x);
    int len = vsnprintf(buf, sizeof(buf), "use to test args: first %d, second %d, thrid %d", ap);
    va_end(ap);
    if (len < 0) {
        std::cout << len << std::endl;
        return;
    }
    std::vector<char> res;
    res.reserve(64);
    res.insert(res.end(), buf, buf + len);
    for (int i = 0; i < len; i++) {
        std::cout << res[i] << " ";
    }
    std::cout << std::endl;
    return;
}


void getTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm time_buf;
    localtime_r(&time_t_now, &time_buf);
    char res[32];
    std::strftime(res, sizeof(res), "%Y-%m-%d %H:%M:%S", &time_buf);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::cout << res << "." << (int)ms.count() << std::endl;
}

int main() {
#ifdef DEBUG
    std::cout << "Debug模式开启" << std::endl;
#endif
#ifdef RELEASE
    std::cout << "Release模式开启" << std::endl;
#endif
    size_t x = std::hash<std::thread::id>{}(std::this_thread::get_id());
    std::cout << x << std::endl;
    fargs(1, 1, 2, 3);
    std::cout << "-------------------------------\n";
    getTime();
    return 0;
}
