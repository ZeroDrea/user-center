#if 1
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include "net/EventLoop.h"
#include "net/InetAddr.h"
#include "net/TcpServer.h"
#include "net/Connection.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "utils/Logger.h"
#include "threadpool/ThreadPool.h"
#include "router/Router.h"
#include "service/UserHandler.h"
#include "db/MySQLConnectionPool.h"
#include "auth/TokenManager.h"

std::atomic<EventLoop*> g_loop(nullptr);
std::unique_ptr<thread_pool::ThreadPool> g_threadPool;

void signalHandler(int sig) {
    if (sig == SIGINT && g_loop.load()) {
        std::cout << "Received SIGINT, quitting..." << std::endl;
        g_loop.load()->quit();
    }
}

int main() {
    AsyncLogger::getInstance().init("logs", "TcpServer.log");
    MySQLConnectionPool::getInstance().init("tcp://127.0.0.1:3306", "appuser", "Master@123456", "user_center", 3306, 5, 20);
    TokenManager::init("127.0.0.1", 6379, 20);

    EventLoop loop;
    g_loop.store(&loop);
    signal(SIGINT, signalHandler);

    g_threadPool = std::make_unique<thread_pool::ThreadPool>(thread_pool::ThreadPool::Config(6, 20));

    InetAddr listenAddr("127.0.0.1", 8888);
    TcpServer server(&loop, listenAddr, "TestServer");

    Router router;
    router.addRoute("/user/register", HttpRequest::kPost, handleRegister);
    router.addRoute("/user/login", HttpRequest::kPost, handleLogin);
    router.addRoute("/user/info", HttpRequest::kGet, handleGetUserInfo);
    router.addRoute("/user/logout", HttpRequest::kPost, handleLogout);

    server.setHttpRequestCallback([&router](const ConnectionPtr& conn, const HttpRequest& req){
        g_threadPool->Submit([conn, req, &router](){
            HttpResponse resp;
            router.dispatch(req, resp);

            bool keepAlive = false;
            if (req.getVersion() == "HTTP/1.1") {
                keepAlive = (req.getHeader("Connection") != "close");
            } else {
                keepAlive = (req.getHeader("Connection") == "keep-alive");
            }

            if (keepAlive) {
                resp.setHeader("Connection", "keep-alive");
            } else {
                resp.setHeader("Connection", "close");
            }

            conn->send(resp.serialize());
            if (!keepAlive) {
                conn->shutdown();
            }
        });
    });

    server.start();

    std::cout << "Server running on http://127.0.0.1:8888 with thread pool" << std::endl;
    loop.loop();
    std::cout << "Server stopped." << std::endl;
}

#endif


#if 0
#include <iostream>
#include "db/dbTest.h"

int main() {
    dbTest();
}

#endif


#if 0
#include <iostream>
#include <sw/redis++/redis++.h>

int main() {
    try {
        // 创建Redis连接对象，格式为 tcp://host:port
        auto redis = sw::redis::Redis("tcp://127.0.0.1:6379");
        
        // 发送PING命令测试连通性
        auto pong = redis.ping();
        std::cout << "Redis connection test: " << pong << std::endl;

        // 简单的读写测试
        redis.set("cpp_test_key", "hello_redis");
        auto val = redis.get("cpp_test_key");
        if (val) {
            std::cout << "Read value: " << *val << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
#endif