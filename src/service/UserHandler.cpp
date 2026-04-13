#include "service/UserHandler.h"
#include "service/UserService.h"
#include "utils/Logger.h"
#include "auth/TokenManager.h"
#include "common/ResponseHelper.h"

static int verifyToken(const HttpRequest& req, HttpResponse& resp) {
    std::string auth = req.getHeader("Authorization");
    std::string token;
    if (auth.find("Bearer ") == 0) {
        token = auth.substr(7);
    }

    if (token.empty()) {
        resp.setStatusCode(401);
        ResponseHelper::sendError(resp, ErrorCode::TokenMissing, 401);
        return -1;
    }

    int userId = TokenManager::verifyToken(token);
    if (userId <= 0) {
        ResponseHelper::sendError(resp, ErrorCode::TokenInvalid, 401);
        return -1;
    }
    return userId;
}

void handleRegister(const HttpRequest& req, HttpResponse& resp) {
    LOG_DEBUG("Handle register request from %s", 
        req.getHeader("X-Real-IP").empty() ? "unknown" : req.getHeader("X-Real-IP").c_str());
    if (req.getMethod() != HttpRequest::kPost) {
        ResponseHelper::sendError(resp, ErrorCode::MethodNotAllowed, 405);
        return;
    }
    
    try {
        json body = json::parse(req.getBody());
        if (!body.contains("username") || !body.contains("password")) {
            LOG_WARN("Missing username or password in register request, body: %s", req.getBody().c_str());
            ResponseHelper::sendError(resp, ErrorCode::MissingParam, "Missing username or password", 400);
            return;
        }
        std::string username = body["username"];
        std::string password = body["password"];
        std::string email = body.value("email", "");
        
        LOG_INFO("Register attempt for username: %s", username.c_str());
        auto ret = UserService::registerUser(username, password, email);
        if (ret == ErrorCode::Success) {
            LOG_INFO("User registered successfully: %s", username.c_str());
            json data = {{"username", username}};
            ResponseHelper::sendSuccess(resp, data, 201);
        } else if (ret == ErrorCode::UserExists) {
            LOG_WARN("Registration failed: username already exists - %s", username.c_str());
            ResponseHelper::sendError(resp, ErrorCode::UserExists, "Username already exists", 409);
        } else {
            LOG_WARN("Registration failed ret(%d): Database error %s", ret, username.c_str());
            ResponseHelper::sendError(resp, ErrorCode::DatabaseError, "Database error", 500);
        }
    } catch (const json::parse_error& e) {
        LOG_ERROR("JSON parse error in register: %s, body: %s", e.what(), req.getBody().c_str());
        ResponseHelper::sendError(resp, ErrorCode::InvalidJSON, "Invalid JSON", 400);
    }
}

void handleLogin(const HttpRequest& req, HttpResponse& resp) {
    if (req.getMethod() != HttpRequest::kPost) {
        ResponseHelper::sendError(resp, ErrorCode::MethodNotAllowed, 405);
        return;
    }

    try {
        json body = json::parse(req.getBody());
        if (!body.contains("username") || !body.contains("password")) {
            ResponseHelper::sendError(resp, ErrorCode::MissingParam, "Missing username or password", 400);
            return;
        }

        std::string username = body["username"];
        std::string password = body["password"];
        std::string nickname;
        std::string token;

        auto errCode = UserService::loginUser(username, password, token, nickname);
        if (errCode == ErrorCode::Success && !token.empty()) {
            json data;
            data["token"] = token;
            data["nickname"] = nickname.empty() ? username : nickname;
            ResponseHelper::sendSuccess(resp, data);
        } else {
            ResponseHelper::sendError(resp, errCode);
        }
    } catch (const json::parse_error& e) {
        ResponseHelper::sendError(resp, ErrorCode::InvalidJSON, "Invalid JSON", 400);
    }
}

void handleLogout(const HttpRequest& req, HttpResponse& resp) {
    std::string auth = req.getHeader("Authorization");
    std::string token;
    if (auth.find("Bearer ") == 0) {
        token = auth.substr(7);
    }
    if (token.empty()) {
        ResponseHelper::sendError(resp, ErrorCode::TokenMissing, "Missing token", 400);
        return;
    }
    TokenManager::removeToken(token);
    ResponseHelper::sendSuccess(resp);
}

void handleGetUserInfo(const HttpRequest& req, HttpResponse& resp) {
    if (req.getMethod() != HttpRequest::kGet) {
        ResponseHelper::sendError(resp, ErrorCode::MethodNotAllowed, 405);
        return;
    }

    int userId = verifyToken(req, resp);
    if (userId <= 0) {
        return;
    }

    json outJson;
    auto errCode = UserService::getUserInfoJson(userId, outJson);
    if (errCode != ErrorCode::Success) {
        ResponseHelper::sendError(resp, errCode);
    } else {
        ResponseHelper::sendSuccess(resp, outJson);
    }
}

static bool getUpdateProfile(const json& body, HttpResponse& resp, ProfileUpdate& update) {
    if (body.contains("nickname")) {
        if (!body["nickname"].is_string()) {
            ResponseHelper::sendError(resp, ErrorCode::ParamInvalid, "nickname must be string", 400);
            return false;
        }
        std::string val = body["nickname"];
        if (val.length() > 64) {
            ResponseHelper::sendError(resp, ErrorCode::ParamTooLong, "Nickname too long", 400);
            return false;
        }
        update.nickname = val;
    }

    if (body.contains("avatar_url")) {
        if (!body["avatar_url"].is_string()) {
            ResponseHelper::sendError(resp, ErrorCode::ParamInvalid, "avatar_url must be string", 400);
            return false;
        }
        std::string val = body["avatar_url"];
        if (val.length() > 256) {
            ResponseHelper::sendError(resp, ErrorCode::ParamTooLong, "Avatar URL too long", 400);
            return false;
        }
        update.avatar_url = val;
    }

    if (body.contains("bio")) {
        if (!body["bio"].is_string()) {
            ResponseHelper::sendError(resp, ErrorCode::ParamInvalid, "bio must be string", 400);
            return false;
        }
        std::string val = body["bio"];
        if (val.length() > 500) {
            ResponseHelper::sendError(resp, ErrorCode::ParamTooLong, "Bio too long", 400);
            return false;
        }
        update.bio = val;
    }

    if (body.contains("gender")) {
        if (!body["gender"].is_number_integer()) {
            ResponseHelper::sendError(resp, ErrorCode::ParamInvalid, "gender must be integer", 400);
            return false;
        }
        int val = body["gender"];
        if (val < 0 || val > 2) {
            ResponseHelper::sendError(resp, ErrorCode::ParamInvalid, "Gender must be 0,1,2", 400);
            return false;
        }
        update.gender = val;
    }
    return true;    
}

void handleUpdateProfile(const HttpRequest& req, HttpResponse& resp) {
    if (req.getMethod() != HttpRequest::kPut && req.getMethod() != HttpRequest::kPatch) {
        ResponseHelper::sendError(resp, ErrorCode::MethodNotAllowed, 405);
        return;
    }

    int userId = verifyToken(req, resp);
    if (userId <= 0) return;

    json body;
    try {
        body = json::parse(req.getBody());
    } catch (const json::parse_error& e) {
        ResponseHelper::sendError(resp, ErrorCode::InvalidJSON, "Invalid JSON", 400);
        return;
    }

    ProfileUpdate update;
    if(!getUpdateProfile(body, resp, update)) {
        return;
    }
    
    // 至少有一个字段需要更新
    if (!update.nickname.has_value() && !update.avatar_url.has_value() &&
        !update.bio.has_value() && !update.gender.has_value()) {
        ResponseHelper::sendError(resp, ErrorCode::MissingParam, "No fields to update", 400);
        return;
    }

    auto errCode = UserService::updateProfile(userId, update);
    if (errCode == ErrorCode::Success) {
        json outJson;
        errCode = UserService::getUserInfoJson(userId, outJson);
        if (errCode != ErrorCode::Success) {
            LOG_WARN("Profile updated but failed to fetch user info for userId=%d", userId);
            ResponseHelper::sendSuccess(resp);
        } else {
            ResponseHelper::sendSuccess(resp, outJson);
        }
    } else {
        ResponseHelper::sendError(resp, errCode);
    }
}

void handleChangePassword(const HttpRequest& req, HttpResponse& resp) {
    if (req.getMethod() != HttpRequest::kPut) {
        ResponseHelper::sendError(resp, ErrorCode::MethodNotAllowed, 405);
        return;
    }

    int userId = verifyToken(req, resp);
    if (userId <= 0) return;

    json body;
    try {
        body = json::parse(req.getBody());
    } catch (const json::parse_error& e) {
        ResponseHelper::sendError(resp, ErrorCode::InvalidJSON, "Invalid JSON", 400);
        return;
    }

    if (!body.contains("old_password") || !body["old_password"].is_string() ||
        !body.contains("new_password") || !body["new_password"].is_string()) {
        ResponseHelper::sendError(resp, ErrorCode::MissingParam, "Missing old_password or new_password", 400);
        return;
    }

    std::string oldPassword = body["old_password"];
    std::string newPassword = body["new_password"];
    if (newPassword.length() < 6) {
        ResponseHelper::sendError(resp, ErrorCode::ParamInvalid, "Password too short", 400);
        return;
    }

    ErrorCode errCode = UserService::changePassword(userId, oldPassword, newPassword);
    if (errCode != ErrorCode::Success) {
        ResponseHelper::sendError(resp, errCode);
    } else {
        ResponseHelper::sendSuccess(resp);
    }
}