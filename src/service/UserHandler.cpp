#include <nlohmann/json.hpp>
#include "service/UserHandler.h"
#include "service/UserService.h"
#include "utils/Logger.h"
#include "auth/TokenManager.h"

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
            LOG_WARN("Registration failed ret(%d): invalid parameters for %s", ret, username.c_str());
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

        std::string token = UserService::loginUser(username, password, nickname);
        if (!token.empty()) {
            json respData;
            respData["code"] = 0;
            respData["msg"] = "Login success";
            respData["token"] = token;
            resp.setStatusCode(200);
            resp.setBody(respData.dump());
        } else {
            // TODO：
            // 登录失败：用户名或密码错误，或者数据库错误  暂且统一 401
            json respData = {{"code", 401}, {"msg", "Invalid credentials"}};
            resp.setStatusCode(401);
            resp.setBody(respData.dump());
        }
    } catch (const json::parse_error& e) {
        resp.setStatusCode(400);
        resp.setBody(R"({"code":400,"msg":"Invalid JSON"})");
        resp.setContentType("application/json");
    }
}

void handleLogout(const HttpRequest& req, HttpResponse& resp) {
    // 从 Authorization 头获取 Token
    std::string auth = req.getHeader("Authorization");
    std::string token;
    if (auth.find("Bearer ") == 0) {
        token = auth.substr(7);
    }
    if (token.empty()) {
        resp.setStatusCode(400);
        resp.setBody(R"({"code":400,"msg":"Missing token"})");
        resp.setContentType("application/json");
        return;
    }
    TokenManager::removeToken(token);
    json respData = {{"code", 0}, {"msg", "Logout success"}};
    resp.setStatusCode(200);
    resp.setBody(respData.dump());
    resp.setContentType("application/json");
}

void handleGetUserInfo(const HttpRequest& req, HttpResponse& resp) {
    if (req.getMethod() != HttpRequest::kGet) {
        resp.setStatusCode(405);
        resp.setBody(R"({"code":405,"msg":"Method Not Allowed"})");
        resp.setContentType("application/json");
        return;
    }

    // 从 Authorization 头获取 Token
    std::string auth = req.getHeader("Authorization");
    std::string token;
    if (auth.find("Bearer ") == 0) {
        token = auth.substr(7);
    }
    if (token.empty()) {
        resp.setStatusCode(401);
        resp.setBody(R"({"code":401,"msg":"Missing or invalid token"})");
        resp.setContentType("application/json");
        return;
    }

    int userId = TokenManager::verifyToken(token);
    if (userId <= 0) {
        resp.setStatusCode(401);
        resp.setBody(R"({"code":401,"msg":"Invalid or expired token"})");
        resp.setContentType("application/json");
        return;
    }

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