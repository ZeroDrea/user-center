#include "service/UserHandler.h"
#include "service/UserService.h"
#include "utils/Logger.h"
#include "auth/TokenManager.h"

static int verifyToken(const HttpRequest& req, HttpResponse& resp) {
    std::string auth = req.getHeader("Authorization");
    std::string token;
    if (auth.find("Bearer ") == 0) {
        token = auth.substr(7);
    }

    if (token.empty()) {
        resp.setStatusCode(401);
        resp.setBody(R"({"code":401,"msg":"Missing token"})");
        resp.setContentType("application/json");
        return -1;
    }

    int userId = TokenManager::verifyToken(token);
    if (userId <= 0) {
        resp.setStatusCode(401);
        resp.setBody(R"({"code":401,"msg":"Invalid or expired token"})");
        resp.setContentType("application/json");
        return userId;
    }
    return userId;
}

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

    int userId = verifyToken(req, resp);
    if (userId <= 0) {
        return;
    }

    auto userJson = UserService::getUserInfoJson(userId);
    if (userJson.is_null() || userJson.empty()) {
        resp.setStatusCode(404);
        resp.setBody(R"({"code":404,"msg":"User not found"})");
    } else {
        resp.setStatusCode(200);
        resp.setBody(userJson.dump());
    }
    resp.setContentType("application/json");
}

static bool getUpdateProfile(const json& body, HttpResponse& resp, ProfileUpdate& update) {
    if (body.contains("nickname")) {
        if (!body["nickname"].is_string()) {
            resp.setStatusCode(400);
            resp.setBody(R"({"code":400,"msg":"nickname must be string"})");
            resp.setContentType("application/json");
            return false;
        }
        std::string val = body["nickname"];
        if (val.length() > 64) {
            resp.setStatusCode(400);
            resp.setBody(R"({"code":400,"msg":"Nickname too long"})");
            resp.setContentType("application/json");
            return false;
        }
        update.nickname = val;
    }

    if (body.contains("avatar_url")) {
        if (!body["avatar_url"].is_string()) {
            resp.setStatusCode(400);
            resp.setBody(R"({"code":400,"msg":"avatar_url must be string"})");
            resp.setContentType("application/json");
            return false;
        }
        std::string val = body["avatar_url"];
        if (val.length() > 256) {
            resp.setStatusCode(400);
            resp.setBody(R"({"code":400,"msg":"Avatar URL too long"})");
            resp.setContentType("application/json");
            return false;
        }
        update.avatar_url = val;
    }

    if (body.contains("bio")) {
        if (!body["bio"].is_string()) {
            resp.setStatusCode(400);
            resp.setBody(R"({"code":400,"msg":"bio must be string"})");
            resp.setContentType("application/json");
            return false;
        }
        std::string val = body["bio"];
        if (val.length() > 500) {
            resp.setStatusCode(400);
            resp.setBody(R"({"code":400,"msg":"Bio too long"})");
            resp.setContentType("application/json");
            return false;
        }
        update.bio = val;
    }

    if (body.contains("gender")) {
        if (!body["gender"].is_number_integer()) {
            resp.setStatusCode(400);
            resp.setBody(R"({"code":400,"msg":"gender must be integer"})");
            resp.setContentType("application/json");
            return false;
        }
        int val = body["gender"];
        if (val < 0 || val > 2) {
            resp.setStatusCode(400);
            resp.setBody(R"({"code":400,"msg":"Gender must be 0,1,2"})");
            resp.setContentType("application/json");
            return false;
        }
        update.gender = val;
    }
    return true;    
}

void handleUpdateProfile(const HttpRequest& req, HttpResponse& resp) {
    if (req.getMethod() != HttpRequest::kPut && req.getMethod() != HttpRequest::kPatch) {
        resp.setStatusCode(405);
        resp.setBody(R"({"code":405,"msg":"Method Not Allowed"})");
        resp.setContentType("application/json");
        return;
    }

    int userId = verifyToken(req, resp);
    if (userId <= 0) return;

    json body;
    try {
        body = json::parse(req.getBody());
    } catch (const json::parse_error& e) {
        resp.setStatusCode(400);
        resp.setBody(R"({"code":400,"msg":"Invalid JSON"})");
        resp.setContentType("application/json");
        return;
    }

    ProfileUpdate update;
    if(!getUpdateProfile(body, resp, update)) {
        return;
    }
    
    // 至少有一个字段需要更新
    if (!update.nickname.has_value() && !update.avatar_url.has_value() &&
        !update.bio.has_value() && !update.gender.has_value()) {
        resp.setStatusCode(400);
        resp.setBody(R"({"code":400,"msg":"No fields to update"})");
        resp.setContentType("application/json");
        return;
    }

    bool success = UserService::updateProfile(userId, update);
    if (success) {
        json userJson = UserService::getUserInfoJson(userId);
        json respData;
        respData["code"] = 0;
        respData["msg"] = "Profile updated successfully";
        respData["data"] = userJson;
        resp.setStatusCode(200);
        resp.setBody(respData.dump());
    } else {
        resp.setStatusCode(500);
        resp.setBody(R"({"code":500,"msg":"Database error"})");
    }
    resp.setContentType("application/json");
}

void handleChangePassword(const HttpRequest& req, HttpResponse& resp) {
    if (req.getMethod() != HttpRequest::kPut) {
        resp.setStatusCode(405);
        resp.setBody(R"({"code":405,"msg":"Method Not Allowed"})");
        resp.setContentType("application/json");
        return;
    }

    int userId = verifyToken(req, resp);
    if (userId <= 0) return;

    json body;
    try {
        body = json::parse(req.getBody());
    } catch (const json::parse_error& e) {
        resp.setStatusCode(400);
        resp.setBody(R"({"code":400,"msg":"Invalid JSON"})");
        resp.setContentType("application/json");
        return;
    }

    if (!body.contains("old_password") || !body["old_password"].is_string() ||
        !body.contains("new_password") || !body["new_password"].is_string()) {
        resp.setStatusCode(400);
        resp.setBody(R"({"code":400,"msg":"Missing old_password or new_password"})");
        resp.setContentType("application/json");
        return;
    }

    std::string oldPassword = body["old_password"];
    std::string newPassword = body["new_password"];
    if (newPassword.length() < 6) {
        resp.setStatusCode(400);
        resp.setBody(R"({"code":400,"msg":"Password too short"})");
        resp.setContentType("application/json");
        return;
    }

    bool success = UserService::changePassword(userId, oldPassword, newPassword);
    if (success) {
        json respData = {{"code", 0}, {"msg", "Password changed successfully"}};
        resp.setStatusCode(200);
        resp.setBody(respData.dump());
    } else {
        resp.setStatusCode(401);
        resp.setBody(R"({"code":401,"msg":"Invalid old password"})");
    }
    resp.setContentType("application/json");
}