#ifndef NET_CHANNEL_H
#define NET_CHANNEL_H

#include <functional>
#include <cstdint>
#include <sys/epoll.h>

class EventLoop;  // 前向声明

/**
 * Channel 封装一个文件描述符及其所关注的事件。
 * 它知道如何更新自己的事件状态，并在事件发生时调用对应的回调函数。
 * 每个 Channel 只属于一个 EventLoop。
 */
class Channel {
public:
    // 事件回调类型
    using EventCallback = std::function<void()>;

    /**
     * @param loop 所属的 EventLoop
     * @param fd   要封装的文件描述符（非阻塞）
     */
    Channel(EventLoop* loop, int fd);
    ~Channel();

    // 通知 EventLoop 移除当前 Channel
    void remove();

    // 禁止拷贝和赋值
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    // ------------------------- 事件状态查询 -------------------------
    int fd() const { return fd_; }
    uint32_t events() const { return events_; }
    uint32_t revents() const { return revents_; }
    bool isInEpoll() const { return inEpoll_; }

    // 设置 epoll_wait 返回的实际事件（由 EpollPoller 调用）
    void setRevents(uint32_t rev) { revents_ = rev; }
    void setInEpoll(bool in) { inEpoll_ = in; }

    // 是否没有需要监听的事件（用于判断是否要从 epoll 中删除）
    bool isNoneEvent() const { return events_ == kNoneEvent; }

    // ------------------------- 修改关注事件 -------------------------
    void enableReading()   { events_ |= kReadEvent; update(); }
    void disableReading()  { events_ &= ~kReadEvent; update(); }
    void enableWriting()   { events_ |= kWriteEvent; update(); }
    void disableWriting()  { events_ &= ~kWriteEvent; update(); }
    void disableAll()      { events_ = kNoneEvent; update(); }

    // ------------------------- 设置回调函数 -------------------------
    void setReadCallback(EventCallback cb)  { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    /**
     * 事件处理函数，由 EventLoop 调用。
     * 根据 revents_ 的值调用相应的回调（读、写、错误）。
     */
    void handleEvent();

private:
    // 事件常量
    static constexpr uint32_t kNoneEvent = 0;
    static constexpr uint32_t kReadEvent = EPOLLIN | EPOLLPRI | EPOLLRDHUP;   // EPOLLIN | EPOLLPRI | EPOLLRDHUP
    static constexpr uint32_t kWriteEvent = EPOLLOUT;  // EPOLLOUT

    // 通知 EventLoop 更新当前 Channel 的事件状态
    void update();

    EventLoop* loop_;      // 所属的 EventLoop
    int fd_;               // 关联的文件描述符
    uint32_t events_;      // 当前关注的事件
    uint32_t revents_;     // 实际发生的事件（由 epoll 填充）
    bool inEpoll_;         // 是否已注册到 epoll 中

    // 回调函数
    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback errorCallback_;
};

#endif // NET_CHANNEL_H