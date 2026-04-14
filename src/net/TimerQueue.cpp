#include "net/TimerQueue.h"
#include "net/EventLoop.h"
#include "net/Channel.h"
#include <sys/timerfd.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include "utils/Logger.h"

TimerQueue::TimerQueue(EventLoop* loop) :
    loop_(loop),
    timerFd_(::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)),
    timerChannel_(new Channel(loop, timerFd_)) {
    if (timerFd_ < 0) {
        LOG_ERROR("timerfd_create failed: %s", strerror(errno));
        abort();
    }
    timerChannel_->setReadCallback([this](){ handleRead(); });
    timerChannel_->enableReading();
}

TimerQueue::~TimerQueue() {
    timerChannel_->disableAll();
    timerChannel_->remove();
    ::close(timerFd_);
}

void TimerQueue::runAfter(int milliseconds, TimerCallback cb) {
    addTimerInLoop(milliseconds, std::move(cb), false);
}

void TimerQueue::runEvery(int interval, TimerCallback cb) {
    addTimerInLoop(interval, std::move(cb), true);
}

void TimerQueue::addTimerInLoop(int milliseconds, TimerCallback cb, bool repeat) {
    LOG_INFO("addTimerInLoop: ms=%d, repeat=%d", milliseconds, repeat);
    loop_->runInLoop([this, milliseconds, cb = std::move(cb), repeat](){
        auto expire = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
        timers_.push({expire, repeat ? milliseconds : 0, std::move(cb)});
        resetTimerFd();
    });
}

void TimerQueue::resetTimerFd() {
    if (timers_.empty()) {
        LOG_INFO("resetTimerFd: timers empty, disarm timer");
        struct itimerspec disarm = {};
        timerfd_settime(timerFd_, 0, &disarm, nullptr);
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto diff = timers_.top().expireTime - now;
    int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
    LOG_INFO("resetTimerFd: top expire = %lld, now = %lld, diff ms = %lld", 
         (long long)timers_.top().expireTime.time_since_epoch().count(),
         (long long)now.time_since_epoch().count(),
         (long long)ms);
    if (ms < 0) {
        ms = 0;
    }
    struct itimerspec newValue = {};
    newValue.it_value.tv_sec = ms / 1000; // 取秒
    newValue.it_value.tv_nsec = (ms % 1000) * 1000000;  // 取纳秒
    timerfd_settime(timerFd_, 0, &newValue, nullptr);
}

void TimerQueue::handleRead() {
    LOG_INFO("TimerQueue::handleRead triggered");
    uint64_t exp = 0;
    LOG_INFO("handleRead: timers_.size() before = %zu", timers_.size());
    ssize_t n = ::read(timerFd_, &exp, sizeof(exp));
    if (n != sizeof(exp)) {
        LOG_WARN("timerfd read error: %s", strerror(errno));
        return;
    }

    auto now = std::chrono::steady_clock::now();
    std::vector<TimerCallback> toRun;

    while (!timers_.empty() && timers_.top().expireTime <= now) {
    auto node = timers_.top();
    timers_.pop();
    LOG_INFO("handleRead: pop node expire = %lld, now = %lld", 
             (long long)node.expireTime.time_since_epoch().count(),
             (long long)now.time_since_epoch().count());
    toRun.push_back(node.callback);
    if (node.intervalMs > 0) {
        node.expireTime = now + std::chrono::milliseconds(node.intervalMs);
        LOG_INFO("handleRead: reinsert node expire = %lld", 
                 (long long)node.expireTime.time_since_epoch().count());
        timers_.push(std::move(node));
    }
}
    resetTimerFd();

    for (auto& cb : toRun) {
        cb();
    }
}

