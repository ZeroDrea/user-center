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