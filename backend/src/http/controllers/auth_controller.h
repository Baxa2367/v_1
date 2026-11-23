#pragma once
#include "http/http_utils.h"
#include <drogon/HttpController.h>

namespace lumina {

using http::Callback;
using http::HttpRequestPtr;

class AuthController : public drogon::HttpController<AuthController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::registerUser, "/api/v1/auth/register", drogon::Post);
    ADD_METHOD_TO(AuthController::login, "/api/v1/auth/login", drogon::Post);
    ADD_METHOD_TO(AuthController::refresh, "/api/v1/auth/refresh", drogon::Post);
    ADD_METHOD_TO(AuthController::logout, "/api/v1/auth/logout", drogon::Post);
    ADD_METHOD_TO(AuthController::me, "/api/v1/me", drogon::Get, "lumina::AuthFilter");
    ADD_METHOD_TO(AuthController::updateMe, "/api/v1/me", drogon::Patch, "lumina::AuthFilter");
    ADD_METHOD_TO(AuthController::changePassword, "/api/v1/me/password", drogon::Post,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(AuthController::dashboard, "/api/v1/me/dashboard", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(AuthController::notifications, "/api/v1/notifications", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(AuthController::readAll, "/api/v1/notifications/read-all", drogon::Post,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(AuthController::readOne, "/api/v1/notifications/{id}/read", drogon::Post,
                  "lumina::AuthFilter");
    METHOD_LIST_END

    void registerUser(const HttpRequestPtr& req, Callback&& cb);
    void login(const HttpRequestPtr& req, Callback&& cb);
    void refresh(const HttpRequestPtr& req, Callback&& cb);
    void logout(const HttpRequestPtr& req, Callback&& cb);
    void me(const HttpRequestPtr& req, Callback&& cb);
    void updateMe(const HttpRequestPtr& req, Callback&& cb);
    void changePassword(const HttpRequestPtr& req, Callback&& cb);
    void dashboard(const HttpRequestPtr& req, Callback&& cb);
    void notifications(const HttpRequestPtr& req, Callback&& cb);
    void readAll(const HttpRequestPtr& req, Callback&& cb);
    void readOne(const HttpRequestPtr& req, Callback&& cb);
};

} // namespace lumina

