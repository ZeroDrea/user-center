#include "Connection.h"
#include "Channel.h"
#include "EventLoop.h"
#include "InetAddr.h"
#include <utils/Logger.h>

#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

Connection::Connection(EventLoop* loop, int fd, const InetAddr& peerAddr)
    : loop_(loop),
      fd_(fd),
      channel_(loop, fd),
      peerAddr_(peerAddr) {
    // 设置 Channel 的回调函数
    channel_.setReadCallback([this]() { handleRead(); });
    channel_.setWriteCallback([this]() { handleWrite(); });
    channel_.setErrorCallback([this]() { handleError(); });
    // 初始只关注读事件
    channel_.enableReading();
}

Connection::~Connection() {
    // 确保已经关闭 fd
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

void Connection::send(const std::string& message) {
    send(message.data(), message.size());
}

void Connection::send(const char* data, size_t len) {
    std::string message(data, len);
    if (loop_->isInLoopThread()) {
        sendInLoop(message);
    } else {
        loop_->runInLoop([this, message]() { sendInLoop(message); });
    }
}

void Connection::shutdown() {
    if (loop_->isInLoopThread()) {
        // 关闭写端（优雅关闭）
        if (::shutdown(fd_, SHUT_WR) < 0) {
            LOG_ERROR("Connection::shutdown error: %s", strerror(errno));
        }
    } else {
        loop_->runInLoop([this]() { shutdown(); });
    }
}

void Connection::handleRead() {
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(fd_, &savedErrno);
    if (n < 0) {
        if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK) {
            // 没有更多数据，返回
            return;
        }
        LOG_ERROR("Connection::handleRead error: %s", strerror(savedErrno));
        handleError();
        return;
    } else if (n == 0) {
        // 对端关闭连接
        handleClose();
        return;
    }

    // 有数据可读，调用消息回调（如果设置了）
    if (messageCallback_) {
        messageCallback_(shared_from_this(), inputBuffer_);
    }
}

void Connection::handleWrite() {
    // 如果 outputBuffer_ 为空，则不应该注册写事件，但以防万一，取消写事件并返回
    if (outputBuffer_.readableBytes() == 0) {
        channel_.disableWriting();
        return;
    }

    ssize_t n = ::write(fd_, outputBuffer_.peek(), outputBuffer_.readableBytes());
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 仍然不能写，等下次
            return;
        }
        LOG_ERROR("Connection::handleWrite error: %s", strerror(errno));
        handleError();
        return;
    }
    outputBuffer_.retrieve(static_cast<size_t>(n));
    if (outputBuffer_.readableBytes() == 0) {
        channel_.disableWriting();
    }
}

void Connection::handleError() {
    // 发生错误，关闭连接
    if (closeCallback_) {
        closeCallback_(shared_from_this());
    }
    // 注意：closeCallback_ 可能会销毁当前对象，所以之后不能再访问成员
}

void Connection::handleClose() {
    // 对端关闭，调用关闭回调
    if (closeCallback_) {
        closeCallback_(shared_from_this());
    }
}

void Connection::sendInLoop(const std::string& message) {
    // 将消息追加到输出缓冲区
    outputBuffer_.append(message);
#if 0
    // 尝试直接发送（如果当前没有在等待写事件）
    // 注意：如果已经注册了写事件，说明上次未发送完，不能重复注册，但可以继续尝试写
    // 这里简单尝试一次写，如果写不完再注册写事件
    if (!channel_.isWriting()) {
        ssize_t n = ::write(fd_, outputBuffer_.peek(), outputBuffer_.readableBytes());
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 写缓冲区满，注册写事件
                channel_.enableWriting();
            } else {
                LOG_ERROR("Connection::sendInLoop write error: %s", strerror(errno));
                handleError();
            }
        } else {
            outputBuffer_.retrieve(static_cast<size_t>(n));
            if (outputBuffer_.readableBytes() > 0) {
                // 没有全部写完，注册写事件
                channel_.enableWriting();
            }
        }
    } else {
        // 已经在等待写事件，不需要做额外操作，下次写事件会继续发送
    }
#endif
}

void Connection::removeConnection() {
    // 由 closeCallback_ 调用，通常在 TcpServer 中移除连接并销毁对象
    // 注意：此时应该停止 Channel 的事件监听，并关闭 fd
    channel_.disableAll();
    channel_.remove();
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}