#include "db/MySQLClient.h"
#include "db/dbTest.h"
#include <iostream>
int dbTest() {
    MySQLClient client("tcp://127.0.0.1:3306", "root", "520102", "user_center");
    if (!client.connect()) {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }
    std::cout << "Connected!" << std::endl;

    auto res = client.executeQuery("SELECT 1");
    if (res && res->next()) {
        std::cout << "Result: " << res->getInt(1) << std::endl;
    } else {
        std::cerr << "Query failed or no result" << std::endl;
        return 1;
    }
    return 0;
}