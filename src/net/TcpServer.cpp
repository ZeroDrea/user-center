#include <cassert>
#include "net/TcpServer.h"
#include "net/Acceptor.h"
#include "net/EventLoop.h"
#include "net/Connection.h"
#include "net/InetAddr.h"
#include "utils/Logger.h"

TcpServer::TcpServer(EventLoop* loop, const InetAddr& listenAddr, 
                     const std::string& name, bool reusePort)
    : loop_(loop),
      acceptor_(new Acceptor(loop, listenAddr, reusePort)),
      name_(name) {
    // 设置 Acceptor 的新连接回调
    acceptor_->setNewConnectionCallback(
        [this](int clientFd, const InetAddr& peerAddr) {
            onNewConnection(clientFd, peerAddr);
        });
}

TcpServer::~TcpServer() {
    assert(loop_->isInLoopThread());
    // 关闭所有连接
    for (auto& pair : connections_) {
        auto& conn = pair.second;
        conn->setCloseCallback(nullptr);   // 避免回调中再次操作 map
        conn->shutdown();                  // 主动关闭连接
    }
    // 连接会在 shared_ptr 析构时自动释放
}

void TcpServer::start() {
    LOG_INFO("TcpServer [%s] starting on %s", name_.c_str(), acceptor_->listenAddr().toIpPort().c_str());
    // Acceptor 已经在构造函数中开始监听，这里不需要额外操作
    
    // 启动定时器
    loop_->runEvery(10000, [this]() {
        checkIdleConnections(120); // 每 10 秒检查一次空闲连接，超时时间设置为2分钟
    });
}

void TcpServer::onNewConnection(int clientFd, const InetAddr& peerAddr) {
    // 创建 Connection 对象（工厂方法）
    ConnectionPtr conn = Connection::create(loop_, clientFd, peerAddr);
    
    // 设置回调
    conn->setHttpRequestCallback(httpRequestCallback_);
    conn->setCloseCallback([this](const ConnectionPtr& c) {
        onConnectionClosed(c);
    });
    
    // 保存到映射表
    connections_[clientFd] = conn;
    LOG_INFO("TcpServer: new connection from %s, fd=%d, total connections=%zu",
             peerAddr.toIpPort().c_str(), clientFd, connections_.size());
}

void TcpServer::onConnectionClosed(const ConnectionPtr& conn) {
    int fd = conn->fd();
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        connections_.erase(it);
        LOG_INFO("TcpServer: connection fd=%d closed, remaining connections=%zu",
                 fd, connections_.size());
    }
    conn->removeConnection();
}

void TcpServer::checkIdleConnections(int idleSeconds) {
    // 收集所有连接（避免遍历过程中容器被修改）
    std::vector<ConnectionPtr> conns;
    conns.reserve(connections_.size());
    for (auto& pair : connections_) {
        conns.push_back(pair.second);
    }
    for (auto& conn : conns) {
        conn->checkIdleTimeout(idleSeconds);
    }
}