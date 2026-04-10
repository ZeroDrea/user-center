#ifndef ROUTER_ROUTER_H
#define ROUTER_ROUTER_H

#include <functional>
#include <string>
#include <unordered_map>
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

// 路由处理函数类型：接收 HttpRequest，填充 HttpResponse
using RouteHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

class Router {
public:
    // 注册路由：路径 + HTTP方法 -> 处理函数
    void addRoute(const std::string& path, HttpRequest::Method method, RouteHandler handler);

    // 分发请求：根据请求找到对应处理函数执行，若未找到则填充 404
    void dispatch(const HttpRequest& req, HttpResponse& resp);

private:
    // 路由键：路径 + 方法
    struct RouteKey {
        std::string path;
        HttpRequest::Method method;

        bool operator==(const RouteKey& other) const {
            return path == other.path && method == other.method;
        }
    };

    // 为 RouteKey 提供哈希函数
    struct KeyHash {
        std::size_t operator()(const RouteKey& key) const {
            return std::hash<std::string>()(key.path) ^ (std::hash<int>()(static_cast<int>(key.method)) << 1);
        }
    };

    std::unordered_map<RouteKey, RouteHandler, KeyHash> routes_;
};

#endif // ROUTER_ROUTER_H