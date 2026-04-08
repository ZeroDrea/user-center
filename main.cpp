#if 1
#include <iostream>
#include <signal.h>         // 信号处理
#include <unistd.h>
#include "net/EventLoop.h"
#include "net/InetAddr.h"
#include "net/TcpServer.h"
#include "net/Connection.h"
#include "net/Buffer.h"
#include "utils/Logger.h"

std::atomic<EventLoop*> g_loop(nullptr);

void signalHandler(int sig) {
    if (sig == SIGINT && g_loop.load()) {
        std::cout << "Received SIGINT, quitting..." << std::endl;
        g_loop.load()->quit();
    }
}

void onMessage(const ConnectionPtr& conn, Buffer& buf) {
    std::string msg = buf.retrieveAllAsString();
    if (msg.empty()) return;
    std::cout << "Received: " << msg;
    conn->send(msg);
}

int main() {
    AsyncLogger::getInstance().init("logs", "TcpSever");
    EventLoop loop;
    g_loop.store(&loop);
    signal(SIGINT, signalHandler);

    InetAddr listenAddr("127.0.0.1", 8888);
    TcpServer server(&loop, listenAddr, "EchoServer");
    server.setMessageCallback(onMessage);
    server.start();

    std::cout << "Echo server running on port 8888. Press Ctrl+C to stop." << std::endl;
    loop.loop();
    std::cout << "Server stopped." << std::endl;
    return 0;
}

#endif

