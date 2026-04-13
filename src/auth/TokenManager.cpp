#include "auth/TokenManager.h"
#include <sw/redis++/redis++.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include "utils/Logger.h"

using namespace sw::redis;

std::shared_ptr<Redis> TokenManager::redis_ = nullptr;

void TokenManager::init(const std::string& redisHost, int redisPort, size_t poolSize) {
    ConnectionOptions conn_opts;
    conn_opts.host = redisHost;
    conn_opts.port = redisPort;
    // conn_opts.password = "xxx"; // 密码
    
    ConnectionPoolOptions pool_opts;
    pool_opts.size = poolSize;
    pool_opts.wait_timeout = std::chrono::milliseconds(100);
    
    redis_ = std::make_shared<Redis>(conn_opts, pool_opts);
    LOG_INFO("TokenManager initialized with Redis pool size %zu", poolSize);
}

std::string TokenManager::getKey(const std::string& token) {
    return "token:" + token;
}

std::string TokenManager::generateToken() {
    unsigned char buffer[32];
    if (RAND_bytes(buffer, sizeof(buffer)) != 1) {
        LOG_ERROR("RAND_bytes failed");
        // 降级：使用 std::random_device
        std::random_device rd;
        std::mt19937_64 gen(rd());
        for (size_t i = 0; i < sizeof(buffer)/sizeof(uint64_t); ++i) {
            uint64_t val = gen();
            std::memcpy(buffer + i * sizeof(uint64_t), &val, sizeof(uint64_t));
        }
    }
    std::stringstream ss;
    for (int i = 0; i < 32; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i];
    return ss.str();
}

std::string TokenManager::createToken(int userId, int expireSeconds) {
    if (!redis_) {
        LOG_ERROR("TokenManager not initialized");
        return "";
    }
    std::string token = generateToken();
    std::string key = getKey(token);
    std::string setKey = "user_tokens:" + std::to_string(userId);
    try {
        redis_->setex(key, expireSeconds, std::to_string(userId));
        redis_->sadd(setKey, token);
        redis_->expire(setKey, expireSeconds);
        LOG_INFO("Token created for user %d, expires in %d s", userId, expireSeconds);
        return token;
    } catch (const sw::redis::Error& e) {
        LOG_ERROR("Failed to create token for user %d: %s", userId, e.what());
        return "";
    }
}

int TokenManager::verifyToken(const std::string& token) {
    if (!redis_) {
        LOG_ERROR("TokenManager not initialized");
        return -2;
    }
    if (token.empty()) return -1;
    std::string key = getKey(token);
    try {
        auto val = redis_->get(key);
        if (val) {
            int userId = std::stoi(*val);
            LOG_DEBUG("Token verified for user %d", userId);
            return userId;
        } else {
            LOG_DEBUG("Token not found or expired");
            return -1;
        }
    } catch (const Error& e) {
        LOG_ERROR("Redis get error: %s", e.what());
        return -2;
    }
}

void TokenManager::removeToken(const std::string& token) {
    if (!redis_) {
        return;
    }
    if (token.empty()) {
        return;
    }
    std::string key = getKey(token);
    try {
        auto val = redis_->get(key);
        if (val) {
            int userId = std::stoi(*val);
            std::string setKey = "user_tokens:" + std::to_string(userId);
            redis_->srem(setKey, token);
            redis_->del(key);
        }
    } catch (const Error& e) {
        LOG_ERROR("Failed to remove token: %s", e.what());
    }
}

void TokenManager::removeAllTokensForUser(int userId) {
    if (!redis_) {
        LOG_ERROR("TokenManager not initialized");
        return;
    }

    std::string setKey = "user_tokens:" + std::to_string(userId);
    try {
        std::vector<std::string> tokens;
        redis_->smembers(setKey, std::back_inserter(tokens));
        for (const auto& token : tokens) {
            redis_->del(getKey(token));
        }
        redis_->del(setKey);
        LOG_INFO("All tokens removed for user %d, count=%zu", userId, tokens.size());
    } catch (const sw::redis::Error& e) {
        LOG_ERROR("Redis error in removeAllTokensForUser: %s", e.what());
    }
}