#ifndef AUTH_TOKENMANAGER_H
#define AUTH_TOKENMANAGER_H

#include <string>
#include <memory>
#include <sw/redis++/redis++.h>

class TokenManager {
public:
    // 初始化 Redis 连接池（应在 main 中调用一次）
    static void init(const std::string& redisHost, int redisPort, size_t poolSize = 10);
    
    // 生成 Token 并存储，返回 Token 字符串
    static std::string createToken(int userId, int expireSeconds = 604800);
    
    // 验证 Token，成功返回用户 ID，失败返回 -1（无效/过期），-2（系统错误）
    static int verifyToken(const std::string& token);
    
    // 删除 Token（登出）
    static void removeToken(const std::string& token);

    static void removeAllTokensForUser(int userId);

    static std::shared_ptr<sw::redis::Redis> getRedis() { return redis_; }
    
private:
    static std::string getKey(const std::string& token);
    static std::string generateToken();
    static std::shared_ptr<sw::redis::Redis> redis_;
};

#endif