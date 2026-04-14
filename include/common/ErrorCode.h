#ifndef COMMON_ERRORCODE_H
#define COMMON_ERRORCODE_H

enum class ErrorCode {
    Success = 0,                // 操作成功
    RateLimitExceeded = 429,    // 请求过于频繁

    InvalidJSON = 1001,         // 请求体不是合法的 JSON 格式
    MissingParam = 1002,        // 缺少必要的参数（如 username、password 等）
    ParamTooLong = 1003,        // 参数长度超过限制（如昵称超过 64 字符）
    ParamInvalid = 1004,        // 参数值无效（如性别不是 0/1/2）
    MethodNotAllowed = 1005,    // HTTP 方法不允许（如用 GET 访问注册接口）

    InvalidCredentials = 2001,  // 用户名或密码错误
    Unauthorized = 2002,        // 未认证（未提供有效凭证或凭证错误）
    TokenMissing = 2003,        // 请求缺少 Token（Authorization 头为空）
    TokenExpired = 2004,        // Token 已过期
    TokenInvalid = 2005,        // Token 无效（签名错误、格式错误或被删除）
    OldPasswordWrong = 2006,    // 修改密码时，旧密码不正确

    UserNotFound = 3001,        // 用户不存在
    UserExists = 3002,          // 用户已存在（注册时用户名或邮箱重复）
    PermissionDenied = 3003,    // 无权限操作（如尝试修改他人资料）

    DatabaseError = 5001,       // 数据库错误（连接失败、查询超时、SQL 异常等）
    RedisError = 5002,          // Redis 错误（连接失败、命令执行异常）
    InternalError = 5003,       // 内部服务器错误（未分类的运行时错误）
};

inline const char* getErrorMessage(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success: return "success";
        case ErrorCode::RateLimitExceeded: return "Too many requests, please slow down";
        case ErrorCode::InvalidJSON: return "Invalid JSON format";
        case ErrorCode::MissingParam: return "Missing required parameter";
        case ErrorCode::ParamTooLong: return "Parameter too long";
        case ErrorCode::ParamInvalid: return "Invalid parameter value";
        case ErrorCode::MethodNotAllowed: return "Method Not Allowed";
        case ErrorCode::InvalidCredentials: return "Invalid Credentials";
        case ErrorCode::Unauthorized: return "Unauthorized";
        case ErrorCode::TokenMissing: return "Missing token";
        case ErrorCode::TokenExpired: return "Token expired";
        case ErrorCode::TokenInvalid: return "Invalid token";
        case ErrorCode::OldPasswordWrong: return "Old password is incorrect";
        case ErrorCode::UserNotFound: return "User not found";
        case ErrorCode::UserExists: return "User already exists";
        case ErrorCode::PermissionDenied: return "Permission denied";
        case ErrorCode::DatabaseError: return "Database error";
        case ErrorCode::RedisError: return "Redis error";
        case ErrorCode::InternalError: return "Internal server error";
        default: return "Unknown error";
    }
}

#endif