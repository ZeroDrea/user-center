#ifndef NET_EPOLLPOLLER_H
#define NET_EPOLLPOLLER_H

#include <vector>
#include <sys/epoll.h>

class Channel;  // 前向声明，避免循环包含

/**
 * EpollPoller 封装 epoll 系统调用。
 * 负责 epoll 实例的生命周期、事件注册/修改/删除，以及事件等待。
 */
class EpollPoller {
public:
    EpollPoller();
    ~EpollPoller();

    // 禁止拷贝和赋值
    EpollPoller(const EpollPoller&) = delete;
    EpollPoller& operator=(const EpollPoller&) = delete;

    /**
     * 更新 Channel 的注册状态。
     * 根据 Channel 当前的事件（events()）和是否已在 epoll 中（isInEpoll()），
     * 决定执行 ADD、MOD 或 DEL 操作。
     */
    void updateChannel(Channel* channel);

    /**
     * 从 epoll 中移除 Channel（等价于调用 updateChannel 且 events 为空）。
     */
    void removeChannel(Channel* channel);

    /**
     * 等待事件发生。
     * @param timeoutMs 超时时间（毫秒），-1 表示无限等待，0 表示非阻塞轮询。
     * @return 就绪的 Channel 指针列表。
     */
    std::vector<Channel*> poll(int timeoutMs); 

private:
    // epoll 文件描述符
    int epollfd_;

    // 存放 epoll_wait 返回的事件数组
    using EventList = std::vector<epoll_event>;
    EventList events_;

    // 底层 epoll_ctl 封装
    void update(int operation, Channel* channel);
};

#endif // NET_EPOLLPOLLER_H