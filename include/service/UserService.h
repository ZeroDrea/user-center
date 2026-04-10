#ifndef USER_SERVICE_H
#define USER_SERVICE_H
#include <string>
#include <unordered_map>

struct User {
    std::string username;
    std::string password; // 实际应用需要加密，暂时明文
    std::string email;
};

class UserService {
public:
    // 返回值：0 成功，1 用户名已存在，2 参数错误等
    static int registerUser(const std::string& username, 
                            const std::string& password,
                            const std::string& email);
    static bool userExists(const std::string& username);
    // 后续再添加 login, getUserInfo 等
private:
    static std::unordered_map<std::string, User> users_;
};

#endif // USER_SERVICE_H