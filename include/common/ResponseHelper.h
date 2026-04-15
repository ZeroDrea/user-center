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

    static void sendError(HttpResponse& resp, ErrorCode code) {
        sendError(resp, code, getErrorMessage(code), httpStatusFromErrorCode(code));
    }

    static void sendError(HttpResponse& resp, ErrorCode code, const std::string& customMsg) {
        sendError(resp, code, customMsg, httpStatusFromErrorCode(code));
    }

    static void sendError(HttpResponse& resp, ErrorCode code, int httpStatus) {
        sendError(resp, code, getErrorMessage(code), httpStatus);
    }

    static void sendError(HttpResponse& resp, ErrorCode code, const std::string& customMsg, int httpStatus) {
        json j;
        j["code"] = static_cast<int>(code);
        j["msg"] = customMsg;
        resp.setStatusCode(httpStatus);
        resp.setBody(j.dump());
        resp.setContentType("application/json");
    }
};

#endif