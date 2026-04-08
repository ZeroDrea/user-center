#include <cassert>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <vector>

#include "net/EpollPoller.h"
#include "net/Channel.h"
#include "utils/Logger.h"

EpollPoller::EpollPoller()
    : epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(16) {
    if (epollfd_ < 0) {
        LOG_ERROR("epoll_create1 failed: %s", strerror(errno));
        ::abort();
    }
}

EpollPoller::~EpollPoller() {
    ::close(epollfd_);
}

void EpollPoller::update(int operation, Channel* channel) {
    struct epoll_event event;
    event.data.ptr = channel;
    event.events = channel->events();
    if (::epoll_ctl(epollfd_, operation, channel->fd(), &event) < 0) {
        LOG_ERROR("epoll_ctl error (op=%d, fd=%d): %s", operation, channel->fd(), strerror(errno));
        ::abort();
    }
}

void EpollPoller::updateChannel(Channel* channel) {
    if (channel->isNoneEvent()) {
        if (channel->isInEpoll()) {
            removeChannel(channel);
        }
        return;
    }

    if (channel->isInEpoll()) {
        update(EPOLL_CTL_MOD, channel);
    } else {
        update(EPOLL_CTL_ADD, channel);
        channel->setInEpoll(true);
    }
}

void EpollPoller::removeChannel(Channel* channel) {
    if (channel->isInEpoll()) {
        update(EPOLL_CTL_DEL, channel);
        channel->setInEpoll(false);
    }
}

std::vector<Channel*> EpollPoller::poll(int timeoutMs) {
    int numEvents = ::epoll_wait(epollfd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);
    if (numEvents < 0) {
        if (errno == EINTR) {
            LOG_WARN("epoll_wait interrupted by signal");
            return {};
        }
        LOG_ERROR("epoll_wait error: %s", strerror(errno));
        return {};
    }

    std::vector<Channel*> activeChannels;
    activeChannels.reserve(numEvents);
    for (int i = 0; i < numEvents; ++i) {
        Channel* ch = static_cast<Channel*>(events_[i].data.ptr);
        ch->setRevents(events_[i].events);
        activeChannels.push_back(ch);
    }

    if (numEvents == static_cast<int>(events_.size())) {
        events_.resize(events_.size() * 2);
    }
    return activeChannels;
}