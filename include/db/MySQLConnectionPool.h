#ifndef DB_MYSQLCONNECTIONPOOL_H
#define DB_MYSQLCONNECTIONPOOL_H

#include <memory>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "db/MySQLClient.h"

class MySQLConnectionPool {
public:
    static MySQLConnectionPool& getInstance();

    // 初始化连接池
    void init(const std::string& host, const std::string& user,
              const std::string& password, const std::string& database,
              unsigned int port = 3306, size_t minSize = 5, size_t maxSize = 20);

    // 获取一个连接（自动管理生命周期，归还时自动放回池）
    std::shared_ptr<MySQLClient> getConnection();

    // 关闭所有连接（程序退出时调用）
    void closeAll();

private:
    MySQLConnectionPool() = default;
    ~MySQLConnectionPool();

    // 创建新连接
    std::unique_ptr<MySQLClient> createConnection();

    // 归还连接到池（由 shared_ptr 的删除器调用）
    void returnConnection(MySQLClient* conn);

    std::string host_, user_, password_, database_;
    unsigned int port_;
    size_t minSize_ = 5;
    size_t maxSize_ = 20;
    size_t currentSize_ = 0;   // 当前已创建连接总数

    std::queue<std::unique_ptr<MySQLClient>> idleConnections_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool stop_ = false;
};

#endif // DB_MYSQLCONNECTIONPOOL_H