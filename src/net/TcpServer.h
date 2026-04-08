#ifndef NET_TCPSERVER_H
#define NET_TCPSERVER_H

#include <memory>
#include <unordered_map>
#include <functional>
#include <atomic>

class EventLoop;
class Acceptor;
class InetAddr;
class Connection;
using ConnectionPtr = std::shared_ptr<Connection>;

/**
 * TcpServer 是用户使用的顶层网络服务器类。
 * 它负责：
 *   - 管理 Acceptor（监听新连接）
 *   - 管理所有活动的 Connection 对象（通过 shared_ptr）
 *   - 设置 Acceptor 和 Connection 的回调
 *   - 提供用户业务回调（连接建立、消息到达、连接关闭）
 * 
 * 采用单 Reactor 多线程模型：一个 EventLoop 线程处理所有 I/O，
 * 业务逻辑通过用户提供的消息回调自行决定是否投递给线程池。
 */
class TcpServer {
public:
    // 用户回调类型
    using ConnectionCallback = std::function<void(const ConnectionPtr&)>;
    using MessageCallback = std::function<void(const ConnectionPtr&, Buffer&)>;

    /**
     * 构造函数
     * @param loop       I/O 线程的 EventLoop（主 Reactor）
     * @param listenAddr 监听的本地地址（IP + 端口）
     * @param name       服务器名称（用于日志）
     * @param reusePort  是否开启 SO_REUSEPORT
     */
    TcpServer(EventLoop* loop, const InetAddr& listenAddr, 
              const std::string& name = "", bool reusePort = false);
    ~TcpServer();

    // 禁止拷贝和赋值
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    /// 启动服务器（开始监听，事件循环已经在外部运行）
    void start();

    /// 设置消息到达回调（当收到完整消息时调用）
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }

private:
    // 新连接到达时的回调（由 Acceptor 调用）
    void onNewConnection(int clientFd, const InetAddr& peerAddr);
    // 连接关闭时的回调（由 Connection 调用）
    void onConnectionClosed(const ConnectionPtr& conn);

    EventLoop* loop_;                       // 主 Reactor（I/O 线程）
    std::unique_ptr<Acceptor> acceptor_;    // 接受连接器
    std::string name_;                      // 服务器名称
    MessageCallback messageCallback_;       // 用户消息回调

    // 连接管理
    std::unordered_map<int, ConnectionPtr> connections_;
    std::atomic<int> nextConnId_;           // 用于生成连接 ID（可选）
};

#endif // NET_TCPSERVER_H