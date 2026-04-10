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

std::atomic<EventLoop*> g_loop(nullptr);

void signalHandler(int sig) {
    if (sig == SIGINT && g_loop.load()) {
        std::cout << "Received SIGINT, quitting..." << std::endl;
        g_loop.load()->quit();
    }
}

void onMessage(const ConnectionPtr& conn, const HttpRequest& req) {
    req.getHeadAll();
    HttpResponse resp;
    resp.setStatusCode(200);
    
    // 处理 HEAD 请求
    if (req.getMethod() == HttpRequest::kHead) {
        resp.setBody("");  // HEAD 无 body
    } else {
        resp.setBody("Hello\n");
    }
    
    // 根据 HTTP 版本和 Connection 头决定是否保持连接
    bool keepAlive = false;
    if (req.getVersion() == "HTTP/1.1") {
        keepAlive = (req.getHeader("Connection") != "close");
    } else { // HTTP/1.0
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
}

int main() {
    AsyncLogger::getInstance().init("logs", "TcpServer.log");
    EventLoop loop;
    g_loop.store(&loop);
    signal(SIGINT, signalHandler);

    InetAddr listenAddr("127.0.0.1", 8888);
    TcpServer server(&loop, listenAddr, "TestServer");
    server.setHttpRequestCallback(onMessage);
    server.start();

    std::cout << "HTTP server running on port 8888. Press Ctrl+C to stop." << std::endl;
    loop.loop();
    std::cout << "Server stopped." << std::endl;
    return 0;
}