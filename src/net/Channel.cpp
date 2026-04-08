#include <cassert>
#include <sys/epoll.h>
#include <unistd.h>
#include "net/Channel.h"
#include "net/EventLoop.h"
#include <iostream>
#include "utils/Logger.h"

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(kNoneEvent),
      revents_(0),
      inEpoll_(false) {
}

Channel::~Channel() {
    if (inEpoll_) {
        loop_->removeChannel(this);
        if (fd_ >= 0) {
            LOG_DEBUG("close fd: %d success.", fd_);
            ::close(fd_);
        }
    }
}

void Channel::remove() {
    if (inEpoll_) {
        loop_->removeChannel(this);
    }
}

void Channel::update() {
    loop_->updateChannel(this);
}

void Channel::handleEvent() {
    // 错误或挂起优先处理
    if (revents_ & (EPOLLERR | EPOLLHUP)) {
        if (errorCallback_) errorCallback_();
        return;
    }

    // 可读事件（包括普通数据、带外数据、对端关闭通知）
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {  // 半关闭在readCallback_中统一处理
        if (readCallback_) readCallback_();
    }

    // 可写事件
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) writeCallback_();
    }
}