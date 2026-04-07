#ifndef NET_INETADDR_H
#define NET_INETADDR_H

#include <netinet/in.h>
#include <string>

/**
 * InetAddr 封装 IPv4 网络地址（sockaddr_in）。
 * 提供从 IP 字符串/端口构造、从 sockaddr_in 构造、获取 sockaddr* 等接口。
 */
class InetAddr {
public:
    /// 构造一个未初始化的地址（通常用于接收 accept 结果）
    InetAddr();

    /// 通过 IP 字符串和端口构造（如 "127.0.0.1", 8080）
    InetAddr(const std::string& ip, uint16_t port);

    /// 通过 sockaddr_in 构造（用于 accept 返回的地址）
    explicit InetAddr(const struct sockaddr_in& addr);

    /// 获取原始 sockaddr_in 结构（const 版本）
    const struct sockaddr_in& getSockAddrIn() const { return addr_; }

    /// 获取 sockaddr* 指针（用于系统调用，如 bind、accept）
    const struct sockaddr* getSockAddr() const { return reinterpret_cast<const struct sockaddr*>(&addr_); }

    /// 获取 sockaddr 长度（用于系统调用）
    socklen_t getSockLen() const { return sizeof(addr_); }

    /// 返回 IP 字符串（如 "192.168.1.1"）
    std::string toIp() const;

    /// 返回端口（主机字节序）
    uint16_t toPort() const;

    /// 返回 IP:端口 字符串（如 "192.168.1.1:8080"）
    std::string toIpPort() const;

    bool operator==(const InetAddr& rhs) const;
    bool operator<(const InetAddr& rhs) const;

private:
    struct sockaddr_in addr_;
};

#endif // NET_INETADDR_H