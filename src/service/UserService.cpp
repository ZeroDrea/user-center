#include "service/UserService.h"
#include "db/MySQLConnectionPool.h"
#include <openssl/sha.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <memory>



// 生成简单的 UUID (v4 风格)
static std::string generateUUID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);
    std::stringstream ss;
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) ss << '-';
        else if (i == 14) ss << std::hex << dis2(gen);
        else ss << std::hex << dis(gen);
    }
    return ss.str();
}

// SHA256 哈希（暂时使用，后面换 bcrypt）
static std::string sha256(const std::string& str) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
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
    std::string passwordHash = sha256(password);

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

int UserService::loginUser(
    const std::string& username,
    const std::string& password,
    std::string& outNickname) {
    auto conn = MySQLConnectionPool::getInstance().getConnection();
    if (!conn || !conn->isConnected()) {
        return -3;
    }

    // 查询用户
    auto pstmt = conn->prepareStatement(
        "SELECT id, password_hash FROM users WHERE username = ?");
    if (!pstmt) {
        return -3;
    }
    pstmt->setString(1, username);
    auto res = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());
    if (!res || !res->next()) {
        return -1; // 用户名不存在
    }

    int userId = res->getInt("id");
    std::string storedHash = res->getString("password_hash");

    // 验证密码（此处使用 SHA256，后面使用 bcrypt 的验证函数）
    if (sha256(password) != storedHash) {
        return -2; // 密码错误
    }

    // 获取昵称（从 user_profiles 表）
    auto pstmtNick = conn->prepareStatement(
        "SELECT nickname FROM user_profiles WHERE user_id = ?");
    if (pstmtNick) {
        pstmtNick->setInt(1, userId);
        auto nickRes = std::unique_ptr<sql::ResultSet>(pstmtNick->executeQuery());
        if (nickRes && nickRes->next()) {
            outNickname = nickRes->getString("nickname");
        }
    }

    // 更新最后登录时间和 IP（IP 在 Handler 中更新，这里先不更新）
    // 可以在这里调用一个单独的方法更新 last_login_at，但为了避免事务复杂，暂不处理

    return userId; // 成功返回用户ID
}

std::string UserService::getUserInfoJson(int userId) {
    auto conn = MySQLConnectionPool::getInstance().getConnection();
    if (!conn || !conn->isConnected()) {
        return "{}";
    }

    auto pstmt = conn->prepareStatement(
        "SELECT u.uuid, u.username, u.email, u.created_at, "
        "p.nickname, p.avatar_url, p.bio, p.gender "
        "FROM users u LEFT JOIN user_profiles p ON u.id = p.user_id "
        "WHERE u.id = ?");
    if (!pstmt) {
        return "{}";
    }
    pstmt->setInt(1, userId);
    auto res = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());
    if (!res || !res->next()) {
        return "{}";
    }

    // 构造 JSON 字符串（简单拼接，后续可用 nlohmann/json 简化）
    std::string json = "{";
    json += "\"uuid\":\"" + res->getString("uuid") + "\",";
    json += "\"username\":\"" + res->getString("username") + "\",";
    json += "\"email\":\"" + res->getString("email") + "\",";
    json += "\"created_at\":\"" + res->getString("created_at") + "\",";
    json += "\"nickname\":\"" + (res->isNull("nickname") ? "" : res->getString("nickname")) + "\",";
    json += "\"avatar_url\":\"" + (res->isNull("avatar_url") ? "" : res->getString("avatar_url")) + "\",";
    json += "\"bio\":\"" + (res->isNull("bio") ? "" : res->getString("bio")) + "\",";
    json += "\"gender\":" + std::to_string(res->getInt("gender"));
    json += "}";
    return json;
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