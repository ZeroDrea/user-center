#ifndef NET_CONNECTION_H
#define NET_CONNECTION_H

#include <memory>
#include <string>
#include <functional>
#include "Buffer.h"

class EventLoop;
class Channel;
class InetAddr;

/**
 * 每个 Connection 对象拥有一个独立的 socket fd，并负责：
 *   - 非阻塞读写（通过 Channel 与 EventLoop 交互）
 *   - 输入输出缓冲区管理（inputBuffer_ / outputBuffer_）
 *   - 消息解析与回调（通过 MessageCallback 将完整消息交给上层）
 *   - 跨线程发送（通过 EventLoop::runInLoop 保证线程安全）
 * 
 * 注意：Connection 的生命周期通常由 TcpServer 使用 shared_ptr 管理，
 *       以避免工作线程持有悬空指针。
 */
class Connection : public std::enable_shared_from_this<Connection> {
public:
    using MessageCallback = std::function<void(const std::shared_ptr<Connection>&, Buffer&)>;
    using CloseCallback   = std::function<void(const std::shared_ptr<Connection>&)>;
    using ErrorCallback   = std::function<void(const std::shared_ptr<Connection>&)>;

    /**
     * 构造函数
     * @param loop     所属的 EventLoop（即 I/O 线程）
     * @param fd       已连接的非阻塞 socket fd
     * @param peerAddr 对端地址（用于日志或记录）
     */
    Connection(EventLoop* loop, int fd, const InetAddr& peerAddr);
    ~Connection();

    // 禁止拷贝和赋值
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    /// 发送数据（线程安全，可在任意线程调用）
    void send(const std::string& message);
    void send(const char* data, size_t len);

    /// 关闭连接（线程安全）
    void shutdown();

    /// 设置消息回调（当 inputBuffer_ 中解析出完整消息时调用）
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
    /// 设置连接关闭回调（由 TcpServer 用于移除连接）
    void setCloseCallback(CloseCallback cb)     { closeCallback_ = std::move(cb); }
    /// 设置错误回调（可选）
    void setErrorCallback(ErrorCallback cb)     { errorCallback_ = std::move(cb); }

    /// 获取对端地址（只读）
    const InetAddr& peerAddr() const { return peerAddr_; }

private:
    // 事件处理函数（由 Channel 回调）
    void handleRead();
    void handleWrite();
    void handleError();
    void handleClose();   // 对端关闭

    // 实际发送函数（必须在 I/O 线程中执行）
    void sendInLoop(const std::string& message);

    // 将当前连接从 TcpServer 中移除（触发 closeCallback_）
    void removeConnection();

    EventLoop* loop_;                 // 所属的 EventLoop
    int fd_;                          // socket 文件描述符
    Channel channel_;                 // 封装 fd 的事件通道
    Buffer inputBuffer_;              // 输入缓冲区（从 socket 读入）
    Buffer outputBuffer_;             // 输出缓冲区（待发送数据）
    InetAddr peerAddr_;               // 对端地址

    MessageCallback messageCallback_; // 消息回调（用户业务）
    CloseCallback   closeCallback_;   // 关闭回调（通知 TcpServer）
    ErrorCallback   errorCallback_;   // 错误回调（可选）
};

using ConnectionPtr = std::shared_ptr<Connection>;

#endif // NET_CONNECTION_H