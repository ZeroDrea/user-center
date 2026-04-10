#include "service/UserService.h"
#include <mutex>

std::unordered_map<std::string, User> UserService::users_;
static std::mutex usersMutex;

int UserService::registerUser(
    const std::string& username, 
    const std::string& password, 
    const std::string& email
) {
    if (username.empty() || password.empty()) {
        return 2;
    }
    std::lock_guard<std::mutex> lock(usersMutex);
    if (users_.find(username) != users_.end()) {
        return 1;
    }
    users_[username] = {username, password, email};
    return 0;
}

bool UserService::userExists(const std::string& username) {
    std::lock_guard<std::mutex> lock(usersMutex);
    return users_.find(username) != users_.end();
}