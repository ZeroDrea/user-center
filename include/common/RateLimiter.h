#ifndef COMMON_RATELIMITER_H
#define COMMON_RATELIMITER_H

#include <string>
#include <memory>
#include "auth/TokenManager.h"

class RateLimiter {
public:
    // key: 限流标识
    // limit: 时间窗口内最大请求次数
    // periodSeconds: 时间窗口（秒）
    static bool isAllowed(const std::string& key, int limit, int periodSeconds) {
        auto redis = TokenManager::getRedis();
        if (!redis) {
            // Redis 不可用时，可配置策略：允许或拒绝。这里暂时允许，避免服务中断
            return true;
        }
        std::string countKey = "rate_limit:" + key;
        try {
            long long count = redis->incr(countKey); // 第一次key不存在时，先将其设置为0，再加1
            if (count == 1) {
                redis->expire(countKey, periodSeconds);
            }
            return count <= limit;
        } catch (const std::exception& e) {
            // 异常时允许通过
            return true;
        }
    }
};

#endif