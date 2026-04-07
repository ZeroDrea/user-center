#include "InetAddr.h"
#include <arpa/inet.h>
#include <cstring>

InetAddr::InetAddr() {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_.sin_port = 0;
}

InetAddr::InetAddr(const std::string& ip, uint16_t port) {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    if (::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) <= 0) {
        // 无效 IP，设为 INADDR_ANY
        addr_.sin_addr.s_addr = htonl(INADDR_ANY);
    }
}

InetAddr::InetAddr(const struct sockaddr_in& addr) {
    addr_ = addr;
}

std::string InetAddr::toIp() const {
    char buf[64];
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return buf;
}

uint16_t InetAddr::toPort() const {
    return ntohs(addr_.sin_port);
}

std::string InetAddr::toIpPort() const {
    return toIp() + ":" + std::to_string(toPort());
}

bool InetAddr::operator==(const InetAddr& rhs) const {
    return addr_.sin_addr.s_addr == rhs.addr_.sin_addr.s_addr &&
           addr_.sin_port == rhs.addr_.sin_port;
}

bool InetAddr::operator<(const InetAddr& rhs) const {
    if (addr_.sin_addr.s_addr != rhs.addr_.sin_addr.s_addr)
        return addr_.sin_addr.s_addr < rhs.addr_.sin_addr.s_addr;
    return addr_.sin_port < rhs.addr_.sin_port;
}