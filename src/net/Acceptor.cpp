#include "Acceptor.h"
#include "Channel.h"
#include "EventLoop.h"
#include "InetAddr.h"

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

#include <utils/Logger.h>

// 辅助函数：设置文件描述符为非阻塞 + FD_CLOEXEC
static int setNonBlockAndCloseOnExec(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return -1;
    flags = ::fcntl(fd, F_GETFD, 0);
    if (flags < 0) return -1;
    if (::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) return -1;
    return 0;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddr& listenAddr, bool reusePort)
    : loop_(loop),
      listenFd_(::socket(AF_INET, SOCK_STREAM, 0)),
      channel_(new Channel(loop, listenFd_)),
      listening_(false),
      listenAddr_(listenAddr) {
    if (listenFd_ < 0) {
        LOG_ERROR("Acceptor: socket creation failed: %s", strerror(errno));
        ::abort();
    }

    // 设置 SO_REUSEADDR（允许端口复用，避免 TIME_WAIT 后无法绑定）
    int optval = 1;
    if (::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        LOG_ERROR("Acceptor: setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        ::close(listenFd_);
        ::abort();
    }

    // 设置 SO_REUSEPORT（如果需要）
    if (reusePort) {
#ifdef SO_REUSEPORT
        if (::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
            LOG_WARN("Acceptor: setsockopt SO_REUSEPORT failed: %s", strerror(errno));
        }
#else
        LOG_WARN("Acceptor: SO_REUSEPORT not supported on this platform");
#endif
    }

    // 绑定地址
    const struct sockaddr* addr = listenAddr_.getSockAddr();
    socklen_t addrLen = listenAddr_.getSockLen();
    if (::bind(listenFd_, addr, addrLen) < 0) {
        LOG_ERROR("Acceptor: bind failed: %s", strerror(errno));
        ::close(listenFd_);
        ::abort();
    }

    // 开始监听
    if (::listen(listenFd_, SOMAXCONN) < 0) {
        LOG_ERROR("Acceptor: listen failed: %s", strerror(errno));
        ::close(listenFd_);
        ::abort();
    }

    // 设置为非阻塞
    if (setNonBlockAndCloseOnExec(listenFd_) < 0) {
        LOG_ERROR("Acceptor: setNonBlockAndCloseOnExec failed: %s", strerror(errno));
        ::close(listenFd_);
        ::abort();
    }

    // 设置 Channel 的回调并启用读事件
    channel_->setReadCallback([this]() { handleRead(); });
    channel_->enableReading();

    listening_ = true;
}

Acceptor::~Acceptor() {
    if (listening_) {
        channel_->disableAll();
        channel_->remove();
    }
    if (listenFd_ >= 0) {
        ::close(listenFd_);
    }
}

void Acceptor::setNewConnectionCallback(NewConnectionCallback cb) {
    newConnectionCallback_ = std::move(cb);
}

void Acceptor::handleRead() {
    while (true) {
        struct sockaddr_in peerAddr;
        socklen_t addrLen = sizeof(peerAddr);
        int clientFd = ::accept(listenFd_, (struct sockaddr*)&peerAddr, &addrLen);
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没有更多连接
                break;
            } else if (errno == EMFILE) {
                // 文件描述符耗尽，可记录错误并跳过
                LOG_ERROR("Acceptor: accept failed (EMFILE), too many open files");
                // 建议：关闭一个空闲连接或继续
                break;
            } else {
                LOG_ERROR("Acceptor: accept error: %s", strerror(errno));
                break;
            }
        }

        // 设置 clientFd 为非阻塞 + FD_CLOEXEC
        if (setNonBlockAndCloseOnExec(clientFd) < 0) {
            LOG_ERROR("Acceptor: setNonBlockAndCloseOnExec for clientFd failed");
            ::close(clientFd);
            continue;
        }

        // 调用上层回调
        if (newConnectionCallback_) {
            InetAddr peer(peerAddr);
            newConnectionCallback_(clientFd, peer);
        } else {
            // 没有设置回调，直接关闭连接
            ::close(clientFd);
        }
    }
}