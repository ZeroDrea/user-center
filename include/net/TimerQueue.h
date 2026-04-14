#ifndef NET_TIMERQUEUE_H
#define NET_TIMERQUEUE_H

#include <functional>
#include <queue>
#include <chrono>
#include <memory>
#include <vector>

class EventLoop;
class Channel;
class TimerQueue {
public:
    using TimerCallback = std::function<void()>;
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    // 在 milliseconds 毫秒后执行回调（一次性）
    void runAfter(int milliseconds, TimerCallback cb);

    // 每隔 interval 毫秒后执行回调（周期性）
    void runEvery(int interval, TimerCallback cb);

private:
    struct TimerNode {
        std::chrono::steady_clock::time_point expireTime;
        int intervalMs; // 0 表示一次性，>0 表示周期性
        TimerCallback callback;
        bool operator>(const TimerNode& other) const {
            return expireTime > other.expireTime;
        }

        bool operator<(const TimerNode& other) const {
            return expireTime < other.expireTime;
        }
    };

    void addTimerInLoop(int milliseconds, TimerCallback cb, bool repeat);
    void resetTimerFd();
    void handleRead();

    EventLoop* loop_;
    int timerFd_;
    std::unique_ptr<Channel> timerChannel_;
    std::priority_queue<TimerNode, std::vector<TimerNode>, std::greater<TimerNode>> timers_;
};

#endif // NET_TIMERQUEUE_H