#ifndef NET_EVENTLOOP_H
#define NET_EVENTLOOP_H

#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>

class EpollPoller;
class Channel;

/**
 * 主要职责：
 *   - 运行事件循环，调用 EpollPoller::poll() 获取就绪的 I/O 事件。
 *   - 将就绪的 Channel 的事件分发给相应的回调（读、写、错误）。
 *   - 提供跨线程任务投递能力（runInLoop / queueInLoop），允许其他线程安全地
 *     将任务交给 EventLoop 线程执行。
 *   - 通过 eventfd 实现跨线程唤醒，保证任务队列得到及时处理。
 */
class EventLoop {
public:
    // 任务回调类型（无参、无返回值）
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 禁止拷贝和赋值
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    /**
     * 启动事件循环，阻塞直到调用 quit()。
     * 循环中会执行：
     *   - 调用 EpollPoller::poll() 等待 I/O 事件（超时 10ms）
     *   - 处理就绪 Channel 的回调
     *   - 执行跨线程任务队列中的所有任务
     */
    void loop();

    /**
     * 退出事件循环。
     * 该函数可以跨线程调用，会唤醒 epoll_wait 以便快速退出。
     */
    void quit();

    /**
     * 断言当前线程是 EventLoop 所属线程（调试用）。
     * 在需要确保线程安全的函数中调用。
     */
    void assertInLoopThread() const;

    /**
     * 判断当前线程是否为 EventLoop 所属线程。
     */
    bool isInLoopThread() const;

    /**
     * 更新 Channel 在 epoll 中的注册状态。
     * 由 Channel 调用，转发给 EpollPoller。
     * 必须在 I/O 线程中调用。
     */
    void updateChannel(Channel* channel);

    /**
     * 从 epoll 中移除 Channel。
     * 由 Channel 调用，转发给 EpollPoller。
     * 必须在 I/O 线程中调用。
     */
    void removeChannel(Channel* channel);

    /**
     * 在当前线程执行 Functor。
     * - 如果调用者本身就是 EventLoop 线程，则立即执行。
     * - 否则，将任务放入队列并唤醒 EventLoop 线程，稍后执行。
     */
    void runInLoop(Functor cb);

    /**
     * 将 Functor 放入队列，由 EventLoop 线程在合适的时机执行（总是异步）。
     * 通常由 runInLoop 在跨线程时调用，也可直接使用。
     */
    void queueInLoop(Functor cb);

private:
    // 唤醒 EventLoop 线程（向 eventfd 写入数据）
    void wakeup();

    // 唤醒 Channel 的读回调，读取 eventfd 数据
    void handleWakeup();

    // 执行队列中的所有待处理任务
    void doPendingFunctors();

    // 核心组件
    std::unique_ptr<EpollPoller> poller_;   // I/O 多路复用器

    // 跨线程唤醒机制
    int wakeupFd_;                           // eventfd 文件描述符
    std::unique_ptr<Channel> wakeupChannel_; // 封装 wakeupFd_ 的 Channel

    // 线程标识
    const pid_t threadId_;                   // EventLoop 所属线程 ID

    // 循环控制标志
    std::atomic<bool> quit_;                 // 是否要求退出循环
    bool looping_;                           // 是否正在循环中（用于队列唤醒判断）

    // 跨线程任务队列
    std::vector<Functor> pendingFunctors_;   // 待执行的任务列表
    mutable std::mutex mutex_;               // 保护 pendingFunctors_ 的互斥锁
    bool callingPendingFunctors_;   // 是否正在执行 pending functors
};

#endif // NET_EVENTLOOP_H