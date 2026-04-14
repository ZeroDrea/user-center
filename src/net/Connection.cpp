#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#include "net/Connection.h"
#include "net/Channel.h"
#include "net/EventLoop.h"
#include "net/InetAddr.h"
#include "utils/Logger.h"



std::shared_ptr<Connection> Connection::create(EventLoop* loop, int fd, const InetAddr& peerAddr) {
    return std::shared_ptr<Connection>(new Connection(loop, fd, peerAddr));
}

Connection::Connection(EventLoop* loop, int fd, const InetAddr& peerAddr)
    : loop_(loop),
      fd_(fd),
      channel_(new Channel(loop, fd)),
      peerAddr_(peerAddr),
      lastActiveTime_(std::chrono::steady_clock::now()) {
    // 设置 Channel 的回调函数
    channel_->setReadCallback([this]() { handleRead(); });
    channel_->setWriteCallback([this]() { handleWrite(); });
    channel_->setErrorCallback([this]() { handleError(); });
    // 初始只关注读事件
    channel_->enableReading();
}

void Connection::send(const std::string& message) {
    send(message.data(), message.size());
}

void Connection::send(const char* data, size_t len) {
    if (loop_->isInLoopThread()) {
        sendInLoop(std::string(data, len)); // 减少拷贝
    } else {
        loop_->runInLoop([this, message = std::string(data, len)]() { sendInLoop(message); });
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

void Connection::sendErrorResponse(int statusCode) {
    std::string body;
    std::string statusMsg;
    if (statusCode == 400) {
        body = "Bad Request";
        statusMsg = "Bad Request";
    } else if (statusCode == 404) {
        body = "Not Found";
        statusMsg = "Not Found";
    } else {
        body = "Internal Server Error";
        statusMsg = "Internal Server Error";
    }
    std::string response = "HTTP/1.1 " + std::to_string(statusCode) + " " + statusMsg + "\r\n"
                         + "Content-Length: " + std::to_string(body.size()) + "\r\n"
                         + "Connection: close\r\n"
                         + "\r\n"
                         + body;
    send(response);
}

void Connection::handleRead() {
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(fd_, &savedErrno);
    if (n < 0) {
        LOG_ERROR("Connection::handleRead error: %s", strerror(savedErrno));
        handleError();
        return;
    } else if (n == 0) { // 对端关闭
        LOG_DEBUG("Connection::handleRead read 0");
        handleClose();
        return;
    }
    LOG_DEBUG("Connection::handleRead fd: %d.",fd_);
    updateActiveTime();
    // 使用状态机解析 inputBuffer_
    bool complete = httpContext_.parse(&inputBuffer_);
    if (httpContext_.isError()) {
        // 解析失败，发送 400 并关闭连接
        LOG_ERROR("httpContext fail, fd: %d.", fd_);
        sendErrorResponse(400);
        handleError();
        return;
    }
    if (complete) {
        // 得到一个完整的 HttpRequest
        if (httpRequestCallback_) {
            LOG_DEBUG("--------- Request complete --------");
            httpRequestCallback_(shared_from_this(), httpContext_.getRequest());
        }
        // 重置状态机，准备解析下一个请求（长连接）
        httpContext_.reset();
        // inputBuffer_ 中可能已经包含下一个请求的部分数据，下一次 handleRead 会继续解析
    }
    // 如果 incomplete，什么都不做，等待更多数据
}

void Connection::handleWrite() {
    // 如果 outputBuffer_ 为空，则不应该注册写事件，但以防万一，取消写事件并返回
    if (outputBuffer_.readableBytes() == 0) {
        LOG_DEBUG("Connection::handleWrite: outputBuffer_.readableBytes() == 0");
        channel_->disableWriting();
        return;
    }

    ssize_t n = ::write(fd_, outputBuffer_.peek(), outputBuffer_.readableBytes());
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 仍然不能写，等下次
            LOG_DEBUG("wait for next: %s", strerror(errno));
            return;
        }
        LOG_ERROR("Connection::handleWrite error: %s", strerror(errno));
        handleError();
        return;
    }
    updateActiveTime();
    outputBuffer_.retrieve(static_cast<size_t>(n));
    if (outputBuffer_.readableBytes() == 0) {  // 写完了，取消写事件
        channel_->disableWriting();
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
    // 尝试直接发送（如果当前没有在等待写事件）
    if (!channel_->isWriting()) {
        ssize_t n = ::write(fd_, outputBuffer_.peek(), outputBuffer_.readableBytes());
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 写缓冲区满，注册写事件
                channel_->enableWriting();
            } else {
                LOG_ERROR("Connection::sendInLoop write error: %s", strerror(errno));
                handleError();
            }
        } else {
            updateActiveTime();
            outputBuffer_.retrieve(static_cast<size_t>(n));
            if (outputBuffer_.readableBytes() > 0) {
                // 没有全部写完，注册写事件
                channel_->enableWriting();
            }
        }
    } else {
        // 已经在等待写事件，不需要做额外操作，下次写事件会继续发送
    }
}

void Connection::removeConnection() {
    // 注意：此时应该停止 Channel 的事件监听，并关闭 fd
    channel_->disableAll();
    channel_->remove();
    ::close(fd_);
}

void Connection::updateActiveTime() {
    lastActiveTime_ = std::chrono::steady_clock::now();
}

void Connection::forceClose() {
    loop_->runInLoop([this]() {
        // 触发关闭回调，通知 TcpServer 清理
        if (closeCallback_) {
            closeCallback_(shared_from_this());
        }
    });
}

void Connection::checkIdleTimeout(int timeoutSeconds) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastActiveTime_).count();
    LOG_DEBUG("fd=%d, elapsed=%lld, timeout=%d", fd_, (long long)elapsed, timeoutSeconds);
    if (elapsed >= timeoutSeconds) {
        LOG_DEBUG("idle timeout, fd = %d", fd_);
        loop_->runAfter(0, [self = shared_from_this()]() {
            self->forceClose();
        });
    }
}