#include <iostream>
#include <threadpool/ThreadPool.h>
#include <chrono>
#include <stdarg.h>
#include <vector>
#include "utils/Logger.h"
#include "net/netTest.h"

int main() {
#ifdef DEBUG
    std::cout << "Debug模式开启" << std::endl;
#endif
#ifdef RELEASE
    std::cout << "Release模式开启" << std::endl;
#endif
    
    TestBuffer();
    return 0;
}
