#ifndef NET_INETADDR_H
#define NET_INETADDR_H

#include <string>
#include <arpa/inet.h>

/**
 * 封装 IPv4 地址（IP + Port）
 */
class InetAddr {
public:
    // 空构造（用于临时对象）
    InetAddr();

    // 从 ip:port 构造（ip 为点分字符串，如 "192.168.1.100"）
    InetAddr(const std::string& ip, uint16_t port);

    // 从 sockaddr_in 构造（accept 出来的地址直接用）
    explicit InetAddr(const sockaddr_in& addr);

    // 获取 IP 字符串
    std::string ip() const;

    // 获取端口号
    uint16_t port() const;

    // 获取底层 sockaddr 指针（给 bind/accept 用）
    const sockaddr* getSockAddr() const;
    sockaddr* getSockAddr();

    // 设置地址（内部用）
    void setSockAddr(const sockaddr_in& addr);

private:
    sockaddr_in addr_;
};

#endif // NET_INETADDR_H