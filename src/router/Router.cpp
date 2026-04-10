#include "router/Router.h"

void Router::addRoute(const std::string& path, HttpRequest::Method method, RouteHandler handler) {
    RouteKey key{path, method};
    routes_[key] = std::move(handler);
}

void Router::dispatch(const HttpRequest& req, HttpResponse& resp) {
    // 先尝试原始路径匹配
    RouteKey key{req.getPath(), req.getMethod()};
    auto it = routes_.find(key);
    if (it != routes_.end()) {
        it->second(req, resp);
        return;
    }

    // 如果路径以 '/' 结尾，尝试去掉尾部斜杠再匹配
    const std::string& path = req.getPath();
    if (path.size() > 1 && path.back() == '/') {
        std::string trimmedPath = path.substr(0, path.size() - 1);
        RouteKey key2{trimmedPath, req.getMethod()};
        auto it2 = routes_.find(key2);
        if (it2 != routes_.end()) {
            it2->second(req, resp);
            return;
        }
    }

    // 未匹配任何路由 → 404
    resp.setStatusCode(404);
    resp.setStatusMessage("Not Found");
    resp.setBody("<html><body><h1>404 Not Found</h1></body></html>");
    resp.setContentType("text/html");
}