#include "db/MySQLConnectionPool.h"
#include <iostream>
#include <thread>

#include "utils/Logger.h"

MySQLConnectionPool& MySQLConnectionPool::getInstance() {
    static MySQLConnectionPool instance;
    return instance;
}

MySQLConnectionPool::~MySQLConnectionPool() {
    closeAll();
}

void MySQLConnectionPool::init(
    const std::string& host, const std::string& user,
    const std::string& password, const std::string& database,
    unsigned int port, size_t minSize, size_t maxSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    host_ = host;
    user_ = user;
    password_ = password;
    database_ = database;
    port_ = port;
    minSize_ = minSize;
    maxSize_ = maxSize;
    LOG_INFO("Start to create initial connection," 
        " host(%s), user(%s), database(%s), port(%d), minSize(%zd), maxSize(%zd).",
        host.c_str(), user.c_str(), database.c_str(), port, minSize, maxSize);
    // 创建最小数量的连接
    for (size_t i = 0; i < minSize_; ++i) {
        auto conn = createConnection();
        if (conn) {
            idleConnections_.push(std::move(conn));
            ++currentSize_;
        } else {
            LOG_ERROR("Failed to create initial connection %d.", i + 1);
        }
    }
    LOG_INFO("Mysql Connection Pool init successful, currentSize = %zu.", currentSize_);
}

std::shared_ptr<MySQLClient> MySQLConnectionPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    // 等待直到有空闲连接或者可以创建新连接
    cond_.wait(lock, [this] {
        return stop_ || !idleConnections_.empty() || currentSize_ < maxSize_;
    });

    if (stop_) {
        return nullptr;
    }

    std::unique_ptr<MySQLClient> conn;
    if (!idleConnections_.empty()) {
        conn = std::move(idleConnections_.front());
        idleConnections_.pop();
    } else if (currentSize_ < maxSize_) {
        conn = createConnection();
        if (conn) {
            ++currentSize_;
        } else {
            return nullptr;
        }
    } else {
        return nullptr;
    }

    if (!conn->isConnected()) {
        if (!conn->connect()) {
            // 重连失败，减少计数并返回 nullptr
            --currentSize_;
            return nullptr;
        }
    }

    LOG_INFO("get connection from pool successful, currentSize(%zu).", currentSize_);
    // 使用自定义删除器，确保归还时调用 returnConnection
    return std::shared_ptr<MySQLClient>(conn.release(), [this](MySQLClient* c) {
        this->returnConnection(c);
    });
}

void MySQLConnectionPool::closeAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = true;
    cond_.notify_all();
    while (!idleConnections_.empty()) {
        idleConnections_.pop();
    }
    currentSize_ = 0;
    LOG_INFO("Mysql Connection Pool close successful.");
}

std::unique_ptr<MySQLClient> MySQLConnectionPool::createConnection() {
    auto conn = std::make_unique<MySQLClient>(host_, user_, password_, database_, port_);
    if (conn->connect()) {
        return conn;
    }
    return nullptr;
}

void MySQLConnectionPool::returnConnection(MySQLClient* conn) {
    if (!conn) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_) {
        delete conn;
        --currentSize_;
        return;
    }
  
    // 将连接放回队列
    idleConnections_.push(std::unique_ptr<MySQLClient>(conn));
    LOG_INFO("return connection to pool successful, currentSize(%zu).", currentSize_);
    cond_.notify_one();
}