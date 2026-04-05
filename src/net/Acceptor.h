#ifndef NET_ACCEPTOR_H
#define NET_ACCEPTOR_H

#include <functional>
#include <memory>

class EventLoop;
class Channel;

class Acceptor {
public:

    /**
     * @param loop      所属的 EventLoop
     * @param listenFd  已经创建、绑定并开始监听的 socket 文件描述符
     */
    Acceptor(EventLoop* loop, int listenFd);
    ~Acceptor();

    // 禁止拷贝和赋值
    Acceptor(const Acceptor&) = delete;
    Acceptor& operator=(const Acceptor&) = delete;

    // 是否正在监听（用于状态查询）
    bool listening() const { return listening_; }

private:
    // 当 listen_fd 可读时的处理函数（由 Channel 回调）
    void handleRead();

    EventLoop* loop_;                // 所属的 EventLoop
    int listenFd_;                   // 监听套接字
    std::unique_ptr<Channel> channel_; // 封装 listenFd 的 Channel
    bool listening_;                 // 是否正在监听
};

#endif // NET_ACCEPTOR_H