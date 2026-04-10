#include "http/HttpResponse.h"
#include <unordered_map>
#include <string>

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void HttpResponse::setStatusCode(int code) {
    statusCode_ = code;
    static const std::unordered_map<int, std::string> messages = {
        {200, "OK"},
        {201, "Created"},
        {204, "No Content"},
        {301, "Moved Permanently"},
        {302, "Found"},
        {400, "Bad Request"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {413, "Payload Too Large"},
        {500, "Internal Server Error"},
        {501, "Not Implemented"}
    };
    auto it = messages.find(code);
    statusMessage_ = (it != messages.end()) ? it->second : "Unknown";
}

std::string HttpResponse::serialize() const {
    std::string result = "HTTP/1.1 " + std::to_string(statusCode_) + " " + statusMessage_ + "\r\n";
    
    // 添加头部
    for (const auto& kv : headers_) {
        result += kv.first + ": " + kv.second + "\r\n";
    }
    
    // 自动添加 Content-Length（如果没有且 body 非空）
    if (!body_.empty() && headers_.find("Content-Length") == headers_.end()) {
        result += "Content-Length: " + std::to_string(body_.size()) + "\r\n";
    }
    
    // 自动添加 Content-Type（如果没有且 body 非空）
    if (!body_.empty() && headers_.find("Content-Type") == headers_.end()) {
        result += "Content-Type: text/plain\r\n";
    }
    
    // 自动添加 Server 头（如果没有）
    if (headers_.find("Server") == headers_.end()) {
        result += "Server: MyCppServer/1.0\r\n";
    }
    
    result += "\r\n";
    result += body_;
    return result;
}