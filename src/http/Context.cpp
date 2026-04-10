#include "http/HttpContext.h"
#include "utils/Buffer.h"
#include <strings.h>
#include <cstdlib>

bool HttpContext::parse(Buffer* buf) {
    while (state_ != kGotAll && state_ != kError) {
        if (state_ == kExpectRequestLine) {
            size_t pos = buf->findCRLF();
            if (pos == std::string::npos) break;  // 请求行不完整
            const char* crlf = buf->peek() + pos;
            if (!parseRequestLine(buf->peek(), crlf)) {
                state_ = kError;
                return false;
            }
            buf->retrieve(pos + 2);  // 消费请求行 + \r\n
            state_ = kExpectHeaders;
        }
        else if (state_ == kExpectHeaders) {
            size_t pos = buf->findCRLF();
            if (pos == std::string::npos) break;
            const char* lineStart = buf->peek();
            const char* crlf = lineStart + pos;
            if (pos == 0) {  // 空行：lineStart 和 crlf 指向同一位置
                buf->retrieve(2);  // 跳过 \r\n
                int contentLen = request_.getContentLength();
                if (contentLen > 0) {
                    state_ = kExpectBody;
                } else {
                    state_ = kGotAll;
                }
                continue;
            }
            if (!parseHeader(lineStart, crlf)) {
                state_ = kError;
                return false;
            }
            buf->retrieve(pos + 2);  // 消费这一行 + \r\n
        }
        else if (state_ == kExpectBody) {
            int need = request_.getContentLength();
            if (need <= 0) {
                state_ = kGotAll;
                return true;
            }
            if (buf->readableBytes() >= static_cast<size_t>(need)) {
                std::string body(buf->peek(), need);
                request_.setBody(body);
                buf->retrieve(need);
                state_ = kGotAll;
                return true;
            }
            break;  // body 不足，等待更多数据
        }
    }
    return state_ == kGotAll;
}

bool HttpContext::parseRequestLine(const char* begin, const char* end) {
    std::string line(begin, end - begin);
    size_t pos1 = line.find(' ');
    if (pos1 == std::string::npos) return false;
    std::string methodStr = line.substr(0, pos1);
    
    size_t pos2 = line.find(' ', pos1 + 1);
    if (pos2 == std::string::npos) return false;
    std::string uri = line.substr(pos1 + 1, pos2 - pos1 - 1);
    std::string version = line.substr(pos2 + 1);
    
    // 解析 method
    HttpRequest::Method method = HttpRequest::kInvalid;
    if (methodStr == "GET") method = HttpRequest::kGet;
    else if (methodStr == "POST") method = HttpRequest::kPost;
    else if (methodStr == "PUT") method = HttpRequest::kPut;
    else if (methodStr == "DELETE") method = HttpRequest::kDelete;
    else if (methodStr == "HEAD") method = HttpRequest::kHead;
    else return false;
    request_.setMethod(method);
    
    // 解析 URI: 分离 path 和 query
    size_t qpos = uri.find('?');
    if (qpos != std::string::npos) {
        request_.setPath(uri.substr(0, qpos));
        request_.setQuery(uri.substr(qpos + 1));
    } else {
        request_.setPath(uri);
    }
    
    if (version != "HTTP/1.0" && version != "HTTP/1.1") return false;
    request_.setVersion(version);
    return true;
}

bool HttpContext::parseHeader(const char* begin, const char* end) {
    std::string line(begin, end - begin);
    size_t colon = line.find(':');
    if (colon == std::string::npos) return false;
    std::string key = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    // 去除前导空格
    size_t start = value.find_first_not_of(" \t");
    if (start != std::string::npos) value = value.substr(start);
    // 去除尾部空格
    size_t endpos = value.find_last_not_of(" \t");
    if (endpos != std::string::npos) value = value.substr(0, endpos + 1);
    request_.addHeader(key, value);
    return true;
}