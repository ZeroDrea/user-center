#include "net/TimerQueue.h"
#include "net/EventLoop.h"
#include "net/Channel.h"
#include <sys/timerfd.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include "utils/Logger.h"

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerFd_(::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)),
      timerChannel_(std::make_unique<Channel>(loop, timerFd_)) {
    if (timerFd_ < 0) {
        LOG_ERROR("timerfd_create failed: %s", strerror(errno));
        ::abort();
    }

    timerChannel_->setReadCallback([this]() { handleRead(); });
    timerChannel_->enableReading();
}

TimerQueue::~TimerQueue() {
    timerChannel_->disableAll();
    timerChannel_->remove();
    ::close(timerFd_);
}

void TimerQueue::runAfter(int milliseconds, TimerCallback cb) {
    // 保证线程安全：必须在 IO 线程执行
    loop_->runInLoop([this, milliseconds, cb = std::move(cb)]() mutable {
        addTimerInLoop(milliseconds, std::move(cb), false);
    });
}

void TimerQueue::runEvery(int interval, TimerCallback cb) {
    loop_->runInLoop([this, interval, cb = std::move(cb)]() mutable {
        addTimerInLoop(interval, std::move(cb), true);
    });
}

void TimerQueue::addTimerInLoop(int milliseconds, TimerCallback cb, bool repeat) {
    TimePoint expire = Clock::now() + std::chrono::milliseconds(milliseconds);
    timers_.push({expire, repeat ? milliseconds : 0, std::move(cb)});
    resetTimerFd();
}

void TimerQueue::resetTimerFd() {
    if (timers_.empty()) {
        struct itimerspec disarm = {};
        if (::timerfd_settime(timerFd_, TFD_TIMER_ABSTIME, &disarm, nullptr) < 0) {
            LOG_ERROR("timerfd_settime fail: %s", strerror(errno));
            ::abort();
        }
        return;
    }
    auto expireTime = timers_.top().expireTime;
    struct itimerspec newValue = {};
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(
        expireTime.time_since_epoch());
    auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(
        expireTime.time_since_epoch() - sec);
    newValue.it_value.tv_sec = sec.count();
    newValue.it_value.tv_nsec = nsec.count();
    if (::timerfd_settime(timerFd_, TFD_TIMER_ABSTIME, &newValue, nullptr) < 0) {
        LOG_ERROR("timerfd_settime fail: %s", strerror(errno));
        ::abort();
    }
    return;
}

void TimerQueue::handleRead() {
    uint64_t exp;
    ssize_t n = ::read(timerFd_, &exp, sizeof(exp));
    if (n != sizeof(exp)) {
        LOG_WARN("TimerQueue::handleRead read error: %s", strerror(errno));
        return;
    }

    TimePoint now = Clock::now();
    std::vector<TimerCallback> toRun;
    while (!timers_.empty() && timers_.top().expireTime <= now) {
        auto node = timers_.top();
        timers_.pop();
        if (node.intervalMs > 0) {
            toRun.push_back(node.callback);
            auto missed = (now - node.expireTime) / std::chrono::milliseconds(node.intervalMs); // 可能已经超了好几个周期
            node.expireTime += (missed + 1) * std::chrono::milliseconds(node.intervalMs);
            timers_.push(std::move(node));
        } else {
            toRun.push_back(std::move(node.callback));
        }
    }
    resetTimerFd();

    for (auto& cb : toRun) {
        cb();
    }
}