#include "db/MySQLClient.h"
#include <iostream>
#include <cppconn/exception.h>
#include "utils/Logger.h"

MySQLClient::MySQLClient(const std::string& host, const std::string& user,
                         const std::string& password, const std::string& database,
                         unsigned int port)
    : host_(host), user_(user), password_(password), database_(database), port_(port),
      driver_(nullptr), connected_(false) {}

MySQLClient::~MySQLClient() {
    disconnect();
}

bool MySQLClient::connect() {
    try {
        driver_ = sql::mysql::get_mysql_driver_instance();
        conn_.reset(driver_->connect(host_, user_, password_));
        conn_->setSchema(database_);
        connected_ = true;
        return true;
    } catch (sql::SQLException& e) {
        LOG_ERROR("MySQL connection error: %s.", e.what());
        return false;
    }
}

void MySQLClient::disconnect() {
    if (conn_) {
        conn_->close();
        conn_.reset();
    }
    connected_ = false;
}

bool MySQLClient::isConnected() const {
    return connected_ && conn_ && conn_->isValid();
}

std::unique_ptr<sql::ResultSet> MySQLClient::executeQuery(const std::string& query) {
    if (!isConnected()) return nullptr;
    try {
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        return std::unique_ptr<sql::ResultSet>(stmt->executeQuery(query));
    } catch (sql::SQLException& e) {
        LOG_ERROR("executeQuery error: %s.", e.what());
        return nullptr;
    }
}

int MySQLClient::executeUpdate(const std::string& query) {
    if (!isConnected()) return -1;
    try {
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        return stmt->executeUpdate(query);
    } catch (sql::SQLException& e) {
        LOG_ERROR("executeUpdate error: %s.", e.what());
        return -1;
    }
}

std::unique_ptr<sql::PreparedStatement> MySQLClient::prepareStatement(const std::string& query) {
    if (!isConnected()) return nullptr;
    try {
        return std::unique_ptr<sql::PreparedStatement>(conn_->prepareStatement(query));
    } catch (sql::SQLException& e) {
        LOG_ERROR("prepareStatement error: %s.", e.what());
        return nullptr;
    }
}

uint64_t MySQLClient::getLastInsertId() {
    if (!isConnected()) return 0;
    try {
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID()"));
        if (res && res->next()) {
            return res->getUInt64(1);
        }
    } catch (sql::SQLException& e) {
        LOG_ERROR("getLastInsertId error: %s.", e.what());
    }
    return 0;
}