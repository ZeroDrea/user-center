#ifndef USER_SERVICE_H
#define USER_SERVICE_H

#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include "common/ErrorCode.h"

using json = nlohmann::json;

struct ProfileUpdate {
    std::optional<std::string> nickname;
    std::optional<std::string> avatar_url;
    std::optional<std::string> bio;
    std::optional<int> gender;
};

class UserService {
public:
    // 注册
    static ErrorCode registerUser(const std::string& username,
                                  const std::string& password,
                                  const std::string& email);

    // 登录：成功时 token 和 nickname 通过输出参数返回
    static ErrorCode loginUser(const std::string& username,
                               const std::string& password,
                               std::string& outToken,
                               std::string& outNickname);

    // 获取用户信息：成功时 json 通过输出参数返回
    static ErrorCode getUserInfoJson(int userId, json& outJson);

    // 检查用户名是否存在（可用于注册前验证）
    static ErrorCode checkUsernameExist(const std::string& username, bool& exists);

    // 更新用户资料
    static ErrorCode updateProfile(int userId, const ProfileUpdate& update);

    // 修改密码
    static ErrorCode changePassword(int userId,
                                    const std::string& oldPassword,
                                    const std::string& newPassword);

private:
    // 内部辅助函数
    static ErrorCode getUserByUsername(const std::string& username,
                                       int& userId,
                                       std::string& passwordHash);
};

#endif // USER_SERVICE_H