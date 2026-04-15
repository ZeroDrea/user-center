#ifndef NET_TIMERQUEUE_H
#define NET_TIMERQUEUE_H

#include <functional>
#include <queue>
#include <chrono>
#include <memory>
#include <vector>

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

class EventLoop;
class Channel;

class TimerQueue {
public:
    using TimerCallback = std::function<void()>;

    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    TimerQueue(const TimerQueue&) = delete;
    TimerQueue& operator=(const TimerQueue&) = delete;

    // 在 milliseconds 毫秒后执行回调（一次性）
    void runAfter(int milliseconds, TimerCallback cb);

    // 每隔 interval 毫秒执行回调（周期性）
    void runEvery(int interval, TimerCallback cb);

private:
    struct TimerNode {
        TimePoint expireTime;
        int intervalMs; // 0 表示一次性，>0 表示周期性
        TimerCallback callback;
        bool operator>(const TimerNode& other) const {
            return expireTime > other.expireTime;
        }
    };

    void addTimerInLoop(int milliseconds, TimerCallback cb, bool repeat);
    void resetTimerFd();
    void handleRead();

    EventLoop* loop_;
    int timerFd_;
    std::unique_ptr<Channel> timerChannel_;
    // 最小堆：最早到期的在堆顶
    std::priority_queue<TimerNode, std::vector<TimerNode>, std::greater<TimerNode>> timers_;
};

#endif // NET_TIMERQUEUE_H