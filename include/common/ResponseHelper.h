#ifndef COMMON_RESPONSEHELPER_H
#define COMMON_RESPONSEHELPER_H

#include "http/HttpResponse.h"
#include "common/ErrorCode.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ResponseHelper {
public:
    static void sendSuccess(HttpResponse& resp, const json& data = json::object(), int httpStatus = 200) {
        json j;
        j["code"] = static_cast<int>(ErrorCode::Success);
        j["msg"] = getErrorMessage(ErrorCode::Success);
        if (!data.empty()) j["data"] = data;
        resp.setStatusCode(httpStatus);
        resp.setBody(j.dump());
        resp.setContentType("application/json");
    }

    static void sendError(HttpResponse& resp, ErrorCode code, int httpStatus = 400) {
        json j;
        j["code"] = static_cast<int>(code);
        j["msg"] = getErrorMessage(code);
        resp.setStatusCode(httpStatus);
        resp.setBody(j.dump());
        resp.setContentType("application/json");
    }

    // 重载：自定义错误消息
    static void sendError(HttpResponse& resp, ErrorCode code, const std::string& customMsg, int httpStatus = 400) {
        json j;
        j["code"] = static_cast<int>(code);
        j["msg"] = customMsg;
        resp.setStatusCode(httpStatus);
        resp.setBody(j.dump());
        resp.setContentType("application/json");
    }
};

#endif