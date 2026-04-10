#if 0
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

std::atomic<EventLoop*> g_loop(nullptr);
std::unique_ptr<thread_pool::ThreadPool> g_threadPool;

void signalHandler(int sig) {
    if (sig == SIGINT && g_loop.load()) {
        std::cout << "Received SIGINT, quitting..." << std::endl;
        g_loop.load()->quit();
    }
}

// 模拟业务处理函数
void handleHome(const HttpRequest& req, HttpResponse& resp) {
    LOG_DEBUG("handleHome");
    resp.setStatusCode(200);
    resp.setStatusMessage("OK");
    resp.setBody("<html><body><h1>Welcome to User Center</h1></body></html>");
    resp.setContentType("text/html");
}

void handleTest(const HttpRequest& req, HttpResponse& resp) {
    resp.setStatusCode(200);
    resp.setBody("Test route works!");
    resp.setContentType("text/plain");
}

int main() {
    AsyncLogger::getInstance().init("logs", "TcpServer.log");
    EventLoop loop;
    g_loop.store(&loop);
    signal(SIGINT, signalHandler);

    g_threadPool = std::make_unique<thread_pool::ThreadPool>(thread_pool::ThreadPool::Config(6, 20));

    InetAddr listenAddr("127.0.0.1", 8888);
    TcpServer server(&loop, listenAddr, "TestServer");

    Router router;
    router.addRoute("/", HttpRequest::kGet, handleHome);
    router.addRoute("/test", HttpRequest::kGet, handleTest);
    router.addRoute("/user/register", HttpRequest::kPost, handleRegister);

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


#if 1
#include <iostream>
#include "db/dbTest.h"

int main() {
    dbTest();
}

#endif