#include <sys/eventfd.h>
#include <unistd.h>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <sys/syscall.h>

#include "net/EventLoop.h"
#include "net/EpollPoller.h"
#include "net/Channel.h"
#include <utils/Logger.h>


// 获取当前线程 ID（Linux 系统调用）
static pid_t getThreadId() {
    return static_cast<pid_t>(::syscall(SYS_gettid));
}

// 创建 eventfd 并设置为非阻塞、执行时关闭
static int createEventfd() {
    int fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (fd < 0) {
        LOG_ERROR("Failed to create eventfd: %s", strerror(errno));
        ::abort();
    }
    return fd;
}

EventLoop::EventLoop()
    : poller_(new EpollPoller()),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      threadId_(getThreadId()),
      quit_(false),
      looping_(false),
      callingPendingFunctors_(false) {
    // 设置唤醒通道的读回调，并启用读事件
    wakeupChannel_->setReadCallback([this]() { handleWakeup(); });
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    // 确保事件循环已经退出，否则可能仍有 Channel 引用
    assert(!looping_);
    // 清理唤醒通道
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
}

void EventLoop::loop() {
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;
    LOG_INFO("start EventLoop, threadId: %d, wakeupFd: %d", threadId_, wakeupFd_);

    while (!quit_) {
        // 等待 I/O 事件，超时 10 毫秒（可调整）
        std::vector<Channel*> activeChannels = poller_->poll(10);
        for (Channel* ch : activeChannels) {
            ch->handleEvent();
        }
        // 处理跨线程任务队列
        doPendingFunctors();
    }

    looping_ = false;
    LOG_INFO("EventLoop(%d) quit success!!", threadId_);
}

void EventLoop::quit() {
    quit_ = true;
    // 如果不是本 loop 线程，需要唤醒 epoll_wait
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::assertInLoopThread() const {
    if (!isInLoopThread()) {
        LOG_ERROR("EventLoop::assertInLoopThread failed: not in loop thread");
        ::abort();
    }
}

bool EventLoop::isInLoopThread() const {
    return threadId_ == getThreadId();
}

void EventLoop::updateChannel(Channel* channel) {
    assertInLoopThread();
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
    assertInLoopThread();
    poller_->removeChannel(channel);
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();   // 直接执行
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }
    // 需要唤醒的情况：
    // 1. 不是本 I/O 线程调用（跨线程）
    // 2. 本线程调用，但正在执行 doPendingFunctors（希望新任务在本轮执行）
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("EventLoop::wakeup() write error");
    }
}

void EventLoop::handleWakeup() {
    uint64_t one;
    ssize_t n = ::read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("EventLoop::handleWakeup() read error");
    }
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }
    callingPendingFunctors_ = true;
    for (const Functor& f : functors) {
        f();
    }
    callingPendingFunctors_ = false;
}