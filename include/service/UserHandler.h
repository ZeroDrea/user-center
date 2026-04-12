#ifndef USER_HANDLER_H
#define USER_HANDLER_H

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

void handleRegister(const HttpRequest& req, HttpResponse& resp);
void handleLogin(const HttpRequest& req, HttpResponse& resp);
void handleLogout(const HttpRequest& req, HttpResponse& resp);
void handleGetUserInfo(const HttpRequest& req, HttpResponse& resp);

#endif // USER_HANDLER_H