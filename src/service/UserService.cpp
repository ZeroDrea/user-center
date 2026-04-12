#include "service/UserService.h"
#include "db/MySQLConnectionPool.h"
#include <openssl/rand.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include "auth/TokenManager.h"
#include "bcrypt/BCrypt.hpp"
#include <nlohmann/json.hpp>
#include "utils/Logger.h"

using json = nlohmann::json;


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

int UserService::registerUser(
    const std::string& username,
    const std::string& password,
    const std::string& email) {
    auto conn = MySQLConnectionPool::getInstance().getConnection();
    if (!conn || !conn->isConnected()) {
        return 2; // 数据库错误
    }

    // 开启事务
    conn->executeUpdate("START TRANSACTION");

    // 检查用户名或邮箱是否存在
    auto pstmtCheck = conn->prepareStatement(
        "SELECT id FROM users WHERE username = ? OR email = ?");
    if (!pstmtCheck) {
        conn->executeUpdate("ROLLBACK");
        return 2;
    }
    pstmtCheck->setString(1, username);
    pstmtCheck->setString(2, email);
    auto res = std::unique_ptr<sql::ResultSet>(pstmtCheck->executeQuery());
    if (res && res->next()) {
        conn->executeUpdate("ROLLBACK");
        return 1; // 用户名或邮箱已存在
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
        return 2;
    }
    pstmtInsert->setString(1, uuid);
    pstmtInsert->setString(2, username);
    pstmtInsert->setString(3, email);
    pstmtInsert->setString(4, passwordHash);
    int affected = pstmtInsert->executeUpdate();
    if (affected <= 0) {
        conn->executeUpdate("ROLLBACK");
        return 2;
    }

    uint64_t userId = conn->getLastInsertId();

    // 插入 user_profiles 默认记录
    auto pstmtProfile = conn->prepareStatement(
        "INSERT INTO user_profiles (user_id) VALUES (?)");
    if (!pstmtProfile) {
        conn->executeUpdate("ROLLBACK");
        return 2;
    }
    pstmtProfile->setUInt64(1, userId);
    if (pstmtProfile->executeUpdate() <= 0) {
        conn->executeUpdate("ROLLBACK");
        return 2;
    }

    // 提交事务
    conn->executeUpdate("COMMIT");
    return 0;
}

std::string UserService::loginUser(
    const std::string& username,
    const std::string& password,
    std::string& outNickname) {
    auto conn = MySQLConnectionPool::getInstance().getConnection();
    if (!conn || !conn->isConnected()) {
        // TODO：数据库错误
        return "";
    }
    
    auto pstmt = conn->prepareStatement(
        "SELECT id, password_hash FROM users WHERE username = ?");
    if (!pstmt) {
        // TODO：数据库错误
        return "";
    }
    pstmt->setString(1, username);
    auto res = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());
    if (!res || !res->next()) {
        // TODO：用户名不存在
        return "";
    }

    int userId = res->getInt("id");
    std::string storedHash = res->getString("password_hash");

    if (!BCrypt::validatePassword(password, storedHash)) {
        // TODO：密码错误
        return "";
    }

    auto pstmtNick = conn->prepareStatement(
        "SELECT nickname FROM user_profiles WHERE user_id = ?");
    if (pstmtNick) {
        pstmtNick->setInt(1, userId);
        auto nickRes = std::unique_ptr<sql::ResultSet>(pstmtNick->executeQuery());
        if (nickRes && nickRes->next()) {
            outNickname = nickRes->getString("nickname");
        }
    }

    // TODO：
    // 更新最后登录时间和 IP（IP 在 Handler 中更新，这里先不更新）
    std::string token = TokenManager::createToken(userId);
    return token;
}

std::string UserService::getUserInfoJson(int userId) {
    auto conn = MySQLConnectionPool::getInstance().getConnection();
    if (!conn || !conn->isConnected()) {
        LOG_ERROR("Failed to get MySQL connection");
        return "{}";
    }

    auto pstmt = conn->prepareStatement(
        "SELECT u.uuid, u.username, u.email, u.created_at, "
        "p.nickname, p.avatar_url, p.bio, p.gender "
        "FROM users u LEFT JOIN user_profiles p ON u.id = p.user_id "
        "WHERE u.id = ?");
    if (!pstmt) {
        LOG_ERROR("Prepare statement failed");
        return "{}";
    }
    pstmt->setInt(1, userId);
    auto res = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());
    if (!res || !res->next()) {
        LOG_DEBUG("No user found for userId=%d", userId);
        return "{}";
    }

    json j;
    j["uuid"] = res->getString("uuid");
    j["username"] = res->getString("username");
    j["email"] = res->getString("email");
    j["created_at"] = res->getString("created_at");
    j["nickname"] = res->isNull("nickname") ? "" : res->getString("nickname");
    j["avatar_url"] = res->isNull("avatar_url") ? "" : res->getString("avatar_url");
    j["bio"] = res->isNull("bio") ? "" : res->getString("bio");
    j["gender"] = res->getInt("gender");
    return j.dump();
}

bool UserService::isUsernameExist(const std::string& username) {
    auto conn = MySQLConnectionPool::getInstance().getConnection();
    if (!conn || !conn->isConnected()) {
        return false;
    }
    auto pstmt = conn->prepareStatement("SELECT id FROM users WHERE username = ?");
    if (!pstmt) {
        return false;
    }
    pstmt->setString(1, username);
    auto res = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());
    return (res && res->next());
}

bool UserService::getUserByUsername(const std::string& username, int& userId, std::string& passwordHash) {
    auto conn = MySQLConnectionPool::getInstance().getConnection();
    if (!conn || !conn->isConnected()) {
        return false;
    }
    auto pstmt = conn->prepareStatement("SELECT id, password_hash FROM users WHERE username = ?");
    if (!pstmt) {
        return false;
    }
    pstmt->setString(1, username);
    auto res = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());
    if (res && res->next()) {
        userId = res->getInt("id");
        passwordHash = res->getString("password_hash");
        return true;
    }
    return false;
}