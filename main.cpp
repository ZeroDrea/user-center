#include <iostream>
#include <net/netTest.h>
#include <db/dbTest.h>
#include <service/serviceTest.h>
#include <utils/utilsTest.h>
#include <threadpool/threadpoolTest.h>
int main() {
#ifdef DEBUG
    std::cout << "Debug模式开启" << std::endl;
#endif
#ifdef RELEASE
    std::cout << "Release模式开启" << std::endl;
#endif
    netTest();
    dbTest();
    serviceTest();
    utilsTest();
    threadpoolTest();
    std::cout << "所有模块测试完成......" << std::endl;
    return 0;
}