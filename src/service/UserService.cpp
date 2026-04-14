#include "service/UserService.h"
#include "db/MySQLConnectionPool.h"
#include <openssl/rand.h>
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include "auth/TokenManager.h"
#include "bcrypt/BCrypt.hpp"
#include "utils/Logger.h"

std::string generateUUID() {
    unsigned char uuid[16];  // 128位 = 16字节
    if (RAND_bytes(uuid, sizeof(uuid)) != 1) {
        // 降级：使用 std::random_device + mt19937
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (int i = 0; i < 16; ++i) {
            uuid[i] = static_cast<unsigned char>(dis(gen));
        }
    }
    
    // 设置版本号 (v4) -> 将第7字节的高4位设为 0100 (即 0x40)
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    // 设置变体位 -> 将第9字节的高2位设为 10 (即 0x80)
    uuid[8] = (uuid[8] & 0x3F) | 0x80;
    
    // 格式化为标准 UUID 字符串
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 16; ++i) {
        ss << std::setw(2) << static_cast<int>(uuid[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) ss << '-';
    }
    return ss.str();
}

ErrorCode UserService::registerUser(const std::string& username, const std::string& password, const std::string& email) {
    auto conn = MySQLConnectionPool::getInstance().getConnection();
    if (!conn || !conn->isConnected()) {
        return ErrorCode::DatabaseError;
    }

    conn->executeUpdate("START TRANSACTION");
    auto pstmtCheck = conn->prepareStatement(
        "SELECT id FROM users WHERE username = ? OR email = ?");
    if (!pstmtCheck) {
        conn->executeUpdate("ROLLBACK");
        return ErrorCode::DatabaseError;
    }
    pstmtCheck->setString(1, username);
    pstmtCheck->setString(2, email);
    auto res = std::unique_ptr<sql::ResultSet>(pstmtCheck->executeQuery());
    if (res && res->next()) {
        conn->executeUpdate("ROLLBACK");
        return ErrorCode::UserExists;
    }

    // 生成 uuid 和密码哈希
    std::string uuid = generateUUID();
    std::string passwordHash = BCrypt::generateHash(password, 12); // 成本因子 12

    // 插入 users 表
    auto pstmtInsert = conn->prepareStatement(
        "INSERT INTO users (uuid, username, email, password_hash, status, role) "
        "VALUES (?, ?, ?, ?, 1, 'user')");
    if (!pstmtInsert) {
        conn->executeUpdate("ROLLBACK");
        return ErrorCode::DatabaseError;
    }
    pstmtInsert->setString(1, uuid);
    pstmtInsert->setString(2, username);
    pstmtInsert->setString(3, email);
    pstmtInsert->setString(4, passwordHash);
    int affected = pstmtInsert->executeUpdate();
    if (affected <= 0) {
        conn->executeUpdate("ROLLBACK");
        return ErrorCode::DatabaseError;
    }

    uint64_t userId = conn->getLastInsertId();

    // 插入 user_profiles 默认记录
    auto pstmtProfile = conn->prepareStatement(
        "INSERT INTO user_profiles (user_id) VALUES (?)");
    if (!pstmtProfile) {
        conn->executeUpdate("ROLLBACK");
        return ErrorCode::DatabaseError;
    }
    pstmtProfile->setUInt64(1, userId);
    if (pstmtProfile->executeUpdate() <= 0) {
        conn->executeUpdate("ROLLBACK");
        return ErrorCode::DatabaseError;
    }

    // 提交事务
    conn->executeUpdate("COMMIT");
    return ErrorCode::Success;
}

ErrorCode UserService::loginUser(const std::string& username,
                                 const std::string& password,
                                 std::string& outToken,
                                 std::string& outNickname) {
    auto conn = MySQLConnectionPool::getInstance().getConnection();
    if (!conn || !conn->isConnected()) {
        return ErrorCode::DatabaseError;
    }
    
    auto pstmt = conn->prepareStatement(
        "SELECT id, password_hash FROM users WHERE username = ?");
    if (!pstmt) {
        return ErrorCode::DatabaseError;
    }
    pstmt->setString(1, username);
    auto res = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());
    if (!res || !res->next()) {
        return ErrorCode::InvalidCredentials;
    }

    int userId = res->getInt("id");
    std::string storedHash = res->getString("password_hash");

    if (!BCrypt::validatePassword(password, storedHash)) {
        return ErrorCode::InvalidCredentials;
    }

    auto pstmtNick = conn->prepareStatement(
        "SELECT nickname FROM user_profiles WHERE user_id = ?");
    if (pstmtNick) {
        pstmtNick->setInt(1, userId);
        auto nickRes = std::unique_ptr<sql::ResultSet>(pstmtNick->executeQuery());
        if (nickRes && nickRes->next()) {
            outNickname = nickRes->getString("nickname");
        }
    } else {
        LOG_WARN("mysql select nickname fail, username = %s", username.c_str());
    }

    outToken = TokenManager::createToken(userId);
    return ErrorCode::Success;
}

ErrorCode UserService::getUserInfoJson(int userId, json& outJson) {
    auto redis = TokenManager::getRedis();
    std::string cacheKey = "user:info:" + std::to_string(userId);
    if (redis) {
        try {
            auto cached = redis->get(cacheKey);
            if (cached) {
                if (*cached == "null") {
                    return ErrorCode::UserNotFound;
                }

                outJson = json::parse(*cached);
                LOG_DEBUG("Cache hit for userId=%d", userId);
                return ErrorCode::Success;
            }
        } catch (const std::exception& e) {
            LOG_WARN("Redis get error: %s, fallback to MySQL", e.what());
        }
    }

    // 缓存为命中
    ErrorCode errCode = getUserInfoJsonFromDB(userId, outJson);
    if (errCode != ErrorCode::Success) {
        if (errCode == ErrorCode::UserNotFound) {
            if (redis) {
                try {
                    redis->setex(cacheKey, 60, "null");
                } catch (const std::exception& e) {
                    LOG_WARN("Redis setex error: %s", e.what());
                }
            }
        }
        return errCode;
    }

    // 写入 Redis 缓存（TTL 1小时 + 随机偏移，防止雪崩）
    if (redis) {
        try {
            std::string jsonStr = outJson.dump();
            // 基础过期时间 3600 秒，加上 0~300 秒随机偏移
            thread_local std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
            std::uniform_int_distribution<int> dist(0, 300);
            int ttl = 3600 + dist(rng);
            redis->setex(cacheKey, ttl, jsonStr);
            LOG_DEBUG("Cache set for userId=%d, ttl=%d", userId, ttl);
        } catch (const std::exception& e) {
            LOG_WARN("Redis setex error: %s", e.what());
        }

    }
    return ErrorCode::Success;
}

ErrorCode UserService::getUserInfoJsonFromDB(int userId, json& outJson) {
    auto conn = MySQLConnectionPool::getInstance().getConnection();
    if (!conn || !conn->isConnected()) {
        LOG_ERROR("Failed to get MySQL connection");
        return ErrorCode::DatabaseError;
    }

    auto pstmt = conn->prepareStatement(
        "SELECT u.uuid, u.username, u.email, u.created_at, "
        "p.nickname, p.avatar_url, p.bio, p.gender "
        "FROM users u LEFT JOIN user_profiles p ON u.id = p.user_id "
        "WHERE u.id = ?"
    );
    if (!pstmt) {
        LOG_ERROR("Prepare statement failed");
        return ErrorCode::DatabaseError;
    }
    pstmt->setInt(1, userId);
    auto res = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());
    if (!res || !res->next()) {
        LOG_DEBUG("No user found for userId=%d", userId);
        return ErrorCode::UserNotFound;
    }

    outJson["uuid"] = res->getString("uuid");
    outJson["username"] = res->getString("username");
    outJson["email"] = res->getString("email");
    outJson["created_at"] = res->getString("created_at");
    outJson["nickname"] = res->isNull("nickname") ? "" : res->getString("nickname");
    outJson["avatar_url"] = res->isNull("avatar_url") ? "" : res->getString("avatar_url");
    outJson["bio"] = res->isNull("bio") ? "" : res->getString("bio");
    outJson["gender"] = res->getInt("gender");
    return ErrorCode::Success;
}

ErrorCode UserService::checkUsernameExist(const std::string& username, bool& exists) {
    exists = false;
    auto conn = MySQLConnectionPool::getInstance().getConnection();
    if (!conn || !conn->isConnected()) {
        return ErrorCode::DatabaseError;
    }
    auto pstmt = conn->prepareStatement("SELECT id FROM users WHERE username = ?");
    if (!pstmt) {
        return ErrorCode::DatabaseError;
    }

    pstmt->setString(1, username);
    auto res = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());
    if (!res) {
        return ErrorCode::DatabaseError;
    }
    if (res->next()) {
        exists = true;
    }
    return ErrorCode::Success;
}

ErrorCode UserService::getUserByUsername(const std::string& username,
                                       int& userId,
                                       std::string& passwordHash) {
    auto conn = MySQLConnectionPool::getInstance().getConnection();
    if (!conn || !conn->isConnected()) {
        return ErrorCode::DatabaseError;
    }
    auto pstmt = conn->prepareStatement("SELECT id, password_hash FROM users WHERE username = ?");
    if (!pstmt) {
        return ErrorCode::DatabaseError;
    }
    pstmt->setString(1, username);
    auto res = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());
    if (res && res->next()) {
        userId = res->getInt("id");
        passwordHash = res->getString("password_hash");
        return ErrorCode::Success;
    }
    return ErrorCode::UserNotFound;
}

ErrorCode UserService::updateProfile(int userId, const ProfileUpdate& update) {
    auto conn = MySQLConnectionPool::getInstance().getConnection();
    if (!conn || !conn->isConnected()) {
        LOG_ERROR("Failed to get MySQL connection");
        return ErrorCode::DatabaseError;
    }

    std::vector<std::string> fields;
    if (update.nickname.has_value()) fields.push_back("nickname");
    if (update.avatar_url.has_value()) fields.push_back("avatar_url");
    if (update.bio.has_value()) fields.push_back("bio");
    if (update.gender.has_value()) fields.push_back("gender");

    if (fields.empty()) {
        return ErrorCode::Success;
    }

    std::string setClause;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i != 0) setClause += ", ";
        setClause += fields[i] + " = ?";
    }

    std::string sql = "INSERT INTO user_profiles (user_id, nickname, avatar_url, bio, gender) "
                  "VALUES (?, ?, ?, ?, ?) "
                  "ON DUPLICATE KEY UPDATE " + setClause;

    auto pstmt = conn->prepareStatement(sql);
    if (!pstmt) {
        LOG_ERROR("Prepare statement failed");
        return ErrorCode::DatabaseError;
    }

    int idx = 1;
    pstmt->setInt(idx++, userId);
    pstmt->setString(idx++, update.nickname.has_value() ? *update.nickname : "");
    pstmt->setString(idx++, update.avatar_url.has_value() ? *update.avatar_url : "");
    pstmt->setString(idx++, update.bio.has_value() ? *update.bio : "");
    pstmt->setInt(idx++, update.gender.has_value() ? *update.gender : 0);

    for (const auto& field : fields) {
        if (field == "nickname") {
            pstmt->setString(idx++, *update.nickname);
        } else if (field == "avatar_url") {
            pstmt->setString(idx++, *update.avatar_url);
        } else if (field == "bio") {
            pstmt->setString(idx++, *update.bio);
        } else if (field == "gender") {
            pstmt->setInt(idx++, *update.gender);
        }
    }

    try {
        int affected = pstmt->executeUpdate();
        LOG_INFO("Profile updated for userId=%d, affected rows=%d", userId, affected);
        return ErrorCode::Success;
    } catch (const sql::SQLException& e) {
        LOG_ERROR("MySQL error: %s (errno: %d)", e.what(), e.getErrorCode());
        return ErrorCode::DatabaseError;
    }
}

ErrorCode UserService::changePassword(int userId,
                                    const std::string& oldPassword,
                                    const std::string& newPassword) {
    auto conn = MySQLConnectionPool::getInstance().getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get MySQL connection");
        return ErrorCode::DatabaseError;
    }

    try {
        auto pstmt = conn->prepareStatement("SELECT password_hash FROM users WHERE id = ?");
        if (!pstmt) {
            LOG_ERROR("Prepare statement failed");
            return ErrorCode::DatabaseError;;
        }
        pstmt->setInt(1, userId);
        auto res = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());
        if (!res || !res->next()) {
            LOG_ERROR("User not found for userId=%d", userId);
            return ErrorCode::UserNotFound;
        }
        std::string storedHash = res->getString("password_hash");
        if (!BCrypt::validatePassword(oldPassword, storedHash)) {
            LOG_WARN("Password mismatch for userId=%d", userId);
            return ErrorCode::InvalidCredentials;
        }

        std::string newHash = BCrypt::generateHash(newPassword, 12);
        auto updateStmt = conn->prepareStatement("UPDATE users SET password_hash = ? WHERE id = ?");
        if (!updateStmt) {
            LOG_ERROR("Prepare update statement failed");
            return ErrorCode::DatabaseError;
        }
        updateStmt->setString(1, newHash);
        updateStmt->setInt(2, userId);
        int affected = updateStmt->executeUpdate();

        if (affected > 0) {
            LOG_INFO("Password changed for userId=%d", userId);
            TokenManager::removeAllTokensForUser(userId);
            return ErrorCode::Success;
        } else {
            LOG_INFO("Password unchanged (same as old) for userId=%d", userId);
            return ErrorCode::Success;
        }
    } catch (const sql::SQLException& e) {
        LOG_ERROR("MySQL error in changePassword: %s (errno: %d)", e.what(), e.getErrorCode());
        return ErrorCode::DatabaseError;
    }
}