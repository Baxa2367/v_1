#pragma once
#include "http/http_utils.h"
#include <drogon/HttpController.h>

namespace lumina {

using http::Callback;
using http::HttpRequestPtr;

class SystemController : public drogon::HttpController<SystemController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SystemController::health, "/healthz", drogon::Get);
    ADD_METHOD_TO(SystemController::metrics, "/metrics", drogon::Get);
    ADD_METHOD_TO(SystemController::meta, "/api/v1/meta", drogon::Get);
    ADD_METHOD_TO(SystemController::openapi, "/openapi.yaml", drogon::Get);
    ADD_METHOD_TO(SystemController::docs, "/docs", drogon::Get);
    ADD_METHOD_TO(SystemController::docs, "/docs/", drogon::Get);
    METHOD_LIST_END

    void health(const HttpRequestPtr& req, Callback&& cb);
    void metrics(const HttpRequestPtr& req, Callback&& cb);
    void meta(const HttpRequestPtr& req, Callback&& cb);
    void openapi(const HttpRequestPtr& req, Callback&& cb);
    void docs(const HttpRequestPtr& req, Callback&& cb);
};

} // namespace lumina

