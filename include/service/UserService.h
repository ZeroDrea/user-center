#ifndef USER_SERVICE_H
#define USER_SERVICE_H
#include <string>

class UserService {
public:
    // 注册：返回 0 成功，1 用户名或邮箱已存在，2 数据库错误
    static int registerUser(const std::string& username,
                            const std::string& password,
                            const std::string& email);

    // 登录：返回用户 ID（>0 表示成功），-1 表示用户名不存在，-2 表示密码错误，-3 数据库错误
    static std::string loginUser(const std::string& username,
                         const std::string& password,
                         std::string& outNickname); // 可选输出昵称

    // 获取用户基本信息（JSON 字符串，供 Handler 使用）
    static std::string getUserInfoJson(int userId);

    // 检查用户名是否存在
    static bool isUsernameExist(const std::string& username);

private:
    // 内部辅助函数：根据用户名获取用户 ID 和密码哈希
    static bool getUserByUsername(const std::string& username, int& userId, std::string& passwordHash);
};
#endif // USER_SERVICE_H