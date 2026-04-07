#ifndef NET_ACCEPTOR_H
#define NET_ACCEPTOR_H

#include <functional>
#include <memory>

class EventLoop;
class Channel;
class InetAddr;   // 网络地址封装

/**
 * Acceptor 用于接受新的 TCP 连接。
 * 它封装了 listen_fd 和对应的 Channel，当 listen_fd 可读时，
 * 调用 accept() 获取 client_fd，并通过 NewConnectionCallback 回调传递给上层。
 * Acceptor 只负责生成 client_fd。
 */
class Acceptor {
public:
    // 新连接回调函数类型：参数为 client_fd 和客户端地址
    using NewConnectionCallback = std::function<void(int clientFd, const InetAddr& peerAddr)>;

    /**
     * 构造函数
     * @param loop        所属的 EventLoop（主 Reactor）
     * @param listenAddr  要监听的本地地址（IP + 端口）
     * @param reusePort   是否设置 SO_REUSEPORT（默认 false）
     */
    Acceptor(EventLoop* loop, const InetAddr& listenAddr, bool reusePort = false);
    ~Acceptor();

    // 禁止拷贝和赋值
    Acceptor(const Acceptor&) = delete;
    Acceptor& operator=(const Acceptor&) = delete;

    /// 设置新连接回调（由 TcpServer 调用）
    void setNewConnectionCallback(NewConnectionCallback cb);

    /// 是否正在监听（用于状态查询）
    bool listening() const { return listening_; }

private:
    /// 当 listen_fd 可读时的处理函数（由 Channel 回调）
    void handleRead();

    EventLoop* loop_;                     // 所属的 EventLoop
    int listenFd_;                        // 监听套接字
    std::unique_ptr<Channel> channel_;    // 封装 listenFd 的 Channel
    NewConnectionCallback newConnectionCallback_;  // 新连接回调
    bool listening_;                      // 是否正在监听
};

#endif // NET_ACCEPTOR_H