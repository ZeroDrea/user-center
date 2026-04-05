#include "Channel.h"
#include "EventLoop.h"

#include <cassert>
#include <sys/epoll.h>

Channel::Channel(EventLoop* loop, int fd) :
    loop_(loop),
    fd_(fd),
    events_(kNoneEvent),
    revents_(0),
    inEpoll_(false){

}

Channel::~Channel() {
    if (inEpoll_) {
        disableAll();
    }
}

void Channel::update() {
    loop_->updateChannel(this);
}

void Channel::handleEvent() {
    if (revents_ & ((EPOLLIN | EPOLLPRI))) {
        if (readCallback_) {
            readCallback_();
        }
    }
}

