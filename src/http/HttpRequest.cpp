#include "http/HttpRequest.h"
#include <cstdlib>
#include <cctype>
#include <limits.h>
#include "utils/Logger.h"

int HttpRequest::getContentLength() const {
    auto it = headers_.find("Content-Length");
    if (it == headers_.end()) {
        return -1;   // 没有 Content-Length 头部
    }
    const std::string& value = it->second;
    // 简单的字符串转整数，忽略前导空格
    size_t i = 0;
    while (i < value.size() && std::isspace(value[i])) ++i;
    if (i == value.size()) return -1;
    char* endptr;
    long len = std::strtol(value.c_str() + i, &endptr, 10);
    // 检查转换错误、负数、溢出
    if (endptr == value.c_str() + i || *endptr != '\0' || len < 0 || len > INT_MAX) {
        return -1;
    }
    return static_cast<int>(len);
}

std::string HttpRequest::getHeader(const std::string& key) const {
    // 头部名大小写不敏感，可以存储时统一转为小写，查找时也转小写
    // 这里简化：假设存储时已经用小写键（在 parseHeader 中转换）
    auto it = headers_.find(key);
    if (it != headers_.end()) return it->second;
    // 尝试全小写版本
    std::string lowerKey = key;
    for (char& c : lowerKey) c = std::tolower(c);
    it = headers_.find(lowerKey);
    return (it != headers_.end()) ? it->second : "";
}


void HttpRequest::getHeadAll() const {
    for (const auto& [key, vaule] : headers_) {
        LOG_DEBUG("get header, key : %s, value: %s", key.c_str(), vaule.c_str());
    }
}