#ifndef DB_MYSQLCLIENT_H
#define DB_MYSQLCLIENT_H

#include <memory>
#include <string>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

class MySQLClient {
public:
    MySQLClient(const std::string& host, const std::string& user,
                const std::string& password, const std::string& database,
                unsigned int port = 3306);
    ~MySQLClient();

    bool connect();
    void disconnect();
    bool isConnected() const;

    std::unique_ptr<sql::ResultSet> executeQuery(const std::string& query);
    int executeUpdate(const std::string& query);
    std::unique_ptr<sql::PreparedStatement> prepareStatement(const std::string& query);
    uint64_t getLastInsertId();

private:
    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    unsigned int port_;
    std::unique_ptr<sql::Connection> conn_;
    sql::mysql::MySQL_Driver* driver_;
    bool connected_;
};

#endif // DB_MYSQLCLIENT_H