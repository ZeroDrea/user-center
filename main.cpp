#include <iostream>
#include <signal.h>
#include <unistd.h>
#include "net/EventLoop.h"
#include "net/InetAddr.h"
#include "net/TcpServer.h"
#include "net/Connection.h"
#include "net/Buffer.h"
#include "utils/Logger.h"
#include "threadpool/ThreadPool.h"

std::atomic<EventLoop*> g_loop(nullptr);

void signalHandler(int sig) {
    if (sig == SIGINT && g_loop.load()) {
        std::cout << "Received SIGINT, quitting..." << std::endl;
        g_loop.load()->quit();
    }
}

// 业务处理函数（将在线程池中执行）
void handleBusiness(const ConnectionPtr& conn, std::string msg) {
    // 执行耗时操作，例如数据库查询、计算等
    std::cout << "Processing message in thread: " << std::this_thread::get_id() << std::endl;
    conn->send(msg);
    throw std::runtime_error("something");
}

int main() {

    AsyncLogger::getInstance().init("logs", "TcpSever");
    thread_pool::ThreadPool pool(thread_pool::ThreadPool::Config(6, 10));
    
    EventLoop loop;
    g_loop.store(&loop);
    signal(SIGINT, signalHandler);
    
    InetAddr listenAddr("127.0.0.1", 8888);
    TcpServer server(&loop, listenAddr, "TcpSever");
    
    server.setMessageCallback([&pool](const ConnectionPtr& conn, Buffer& buf) {
        // 将数据从 buf 中取出（拷贝），因为 buf 只能在 I/O 线程访问
        std::string msg = buf.retrieveAllAsString();
        if (msg.empty()) return;
        
        // 将业务处理提交给线程池
        pool.Submit([conn, msg]() {
            try {
                handleBusiness(conn, msg);
            } catch (const std::exception& e) {
                LOG_ERROR("Business error: %s", e.what());
                conn->send("Error\n");
            }
        });
    });
    
    server.start();
    
    std::cout << "Echo server running on port 8888. Press Ctrl+C to stop." << std::endl;
    loop.loop();
    std::cout << "Server stopped." << std::endl;
    return 0;
}