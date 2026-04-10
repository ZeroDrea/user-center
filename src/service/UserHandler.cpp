#include <nlohmann/json.hpp>
#include "service/UserHandler.h"
#include "service/UserService.h"
#include "utils/Logger.h"

using json = nlohmann::json;

void handleRegister(const HttpRequest& req, HttpResponse& resp) {
    LOG_DEBUG("Handle register request from %s", 
        req.getHeader("X-Real-IP").empty() ? "unknown" : req.getHeader("X-Real-IP").c_str());
    if (req.getMethod() != HttpRequest::kPost) {
        LOG_WARN("Register method not allowed: %d", req.getMethod());
        resp.setStatusCode(405);
        resp.setBody(R"({"code":405,"msg":"Method Not Allowed"})");
        resp.setContentType("application/json");
        return;
    }
    
    try {
        json body = json::parse(req.getBody());
        if (!body.contains("username") || !body.contains("password")) {
            LOG_WARN("Missing username or password in register request, body: %s", req.getBody().c_str());
            resp.setStatusCode(400);
            resp.setBody(R"({"code":400,"msg":"Missing username or password"})");
            resp.setContentType("application/json");
            return;
        }
        std::string username = body["username"];
        std::string password = body["password"];
        std::string email = body.value("email", "");
        
        LOG_INFO("Register attempt for username: %s", username.c_str());
        int ret = UserService::registerUser(username, password, email);
        if (ret == 0) {
            LOG_INFO("User registered successfully: %s", username.c_str());
            json respData = {
                {"code", 0},
                {"msg", "User registered successfully"},
                {"username", username}
            };
            resp.setStatusCode(201);
            resp.setBody(respData.dump());
        } else if (ret == 1) {
            LOG_WARN("Registration failed: username already exists - %s", username.c_str());
            json respData = {{"code", 409}, {"msg", "Username already exists"}};
            resp.setStatusCode(409);
            resp.setBody(respData.dump());
        } else {
            LOG_WARN("Registration failed: invalid parameters for %s", username.c_str());
            json respData = {{"code", 400}, {"msg", "Invalid parameters"}};
            resp.setStatusCode(400);
            resp.setBody(respData.dump());
        }
        resp.setContentType("application/json");
    } catch (const json::parse_error& e) {
        LOG_ERROR("JSON parse error in register: %s, body: %s", e.what(), req.getBody().c_str());
        resp.setStatusCode(400);
        resp.setBody(R"({"code":400,"msg":"Invalid JSON"})");
        resp.setContentType("application/json");
    }
}

void handleLogin(const HttpRequest& req, HttpResponse& resp) {
    if (req.getMethod() != HttpRequest::kPost) {
        resp.setStatusCode(405);
        resp.setBody(R"({"code":405,"msg":"Method Not Allowed"})");
        resp.setContentType("application/json");
        return;
    }

    try {
        json body = json::parse(req.getBody());
        if (!body.contains("username") || !body.contains("password")) {
            resp.setStatusCode(400);
            resp.setBody(R"({"code":400,"msg":"Missing username or password"})");
            resp.setContentType("application/json");
            return;
        }

        std::string username = body["username"];
        std::string password = body["password"];
        std::string nickname;

        int ret = UserService::loginUser(username, password, nickname);
        if (ret > 0) {
            json respData;
            respData["code"] = 0;
            respData["msg"] = "Login success";
            respData["user_id"] = ret;
            respData["nickname"] = nickname.empty() ? username : nickname;
            resp.setStatusCode(200);
            resp.setBody(respData.dump());
        } else if (ret == -1) {
            json respData = {{"code", 401}, {"msg", "User not found"}};
            resp.setStatusCode(401);
            resp.setBody(respData.dump());
        } else if (ret == -2) {
            json respData = {{"code", 401}, {"msg", "Invalid password"}};
            resp.setStatusCode(401);
            resp.setBody(respData.dump());
        } else {
            json respData = {{"code", 500}, {"msg", "Database error"}};
            resp.setStatusCode(500);
            resp.setBody(respData.dump());
        }
        resp.setContentType("application/json");
    } catch (const json::parse_error& e) {
        resp.setStatusCode(400);
        resp.setBody(R"({"code":400,"msg":"Invalid JSON"})");
        resp.setContentType("application/json");
    }
}

void handleGetUserInfo(const HttpRequest& req, HttpResponse& resp) {
    // 简化：从请求参数中获取 user_id（实际应从 token 解析）
    // 这里先简单支持通过查询参数 ?user_id=123
    if (req.getMethod() != HttpRequest::kGet) {
        resp.setStatusCode(405);
        resp.setBody(R"({"code":405,"msg":"Method Not Allowed"})");
        resp.setContentType("application/json");
        return;
    }

    // 解析查询参数（例如 /user/info?user_id=123）
    std::string query = req.getQuery();
    // 简单解析：假设格式为 "user_id=123"
    size_t pos = query.find("user_id=");
    if (pos == std::string::npos) {
        resp.setStatusCode(400);
        resp.setBody(R"({"code":400,"msg":"Missing user_id"})");
        resp.setContentType("application/json");
        return;
    }
    std::string userIdStr = query.substr(pos + 8);
    // 可能还有 & 后的参数，截取到 & 或结尾
    size_t end = userIdStr.find('&');
    if (end != std::string::npos) userIdStr = userIdStr.substr(0, end);
    int userId = std::stoi(userIdStr);

    std::string userJson = UserService::getUserInfoJson(userId);
    if (userJson == "{}") {
        resp.setStatusCode(404);
        resp.setBody(R"({"code":404,"msg":"User not found"})");
    } else {
        resp.setStatusCode(200);
        resp.setBody(userJson);
    }
    resp.setContentType("application/json");
}