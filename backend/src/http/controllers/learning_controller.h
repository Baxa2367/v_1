#pragma once
#include "http/http_utils.h"
#include <drogon/HttpController.h>

namespace lumina {

using http::Callback;
using http::HttpRequestPtr;

class LearningController : public drogon::HttpController<LearningController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(LearningController::courses, "/api/v1/courses", drogon::Get, "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::course, "/api/v1/courses/{id}", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::map, "/api/v1/courses/{id}/map", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::enroll, "/api/v1/courses/{id}/enroll", drogon::Post,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::unenroll, "/api/v1/courses/{id}/unenroll", drogon::Post,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::level, "/api/v1/levels/{id}", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::nextTask, "/api/v1/levels/{id}/next-task", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::task, "/api/v1/tasks/{id}", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::submit, "/api/v1/tasks/{id}/attempts", drogon::Post,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::hint, "/api/v1/tasks/{id}/hint", drogon::Post,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::history, "/api/v1/attempts/history", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::continueLearning, "/api/v1/me/continue", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::aiNextTask, "/api/v1/ai/next-task", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::aiExplain, "/api/v1/ai/explain/{id}", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::aiTips, "/api/v1/ai/tips", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::aiForecast, "/api/v1/ai/forecast", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::aiMastery, "/api/v1/ai/mastery", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(LearningController::aiGenerate, "/api/v1/ai/generate", drogon::Post,
                  "lumina::AuthFilter");
    METHOD_LIST_END

    void courses(const HttpRequestPtr& req, Callback&& cb);
    void course(const HttpRequestPtr& req, Callback&& cb);
    void map(const HttpRequestPtr& req, Callback&& cb);
    void enroll(const HttpRequestPtr& req, Callback&& cb);
    void unenroll(const HttpRequestPtr& req, Callback&& cb);
    void level(const HttpRequestPtr& req, Callback&& cb);
    void nextTask(const HttpRequestPtr& req, Callback&& cb);
    void task(const HttpRequestPtr& req, Callback&& cb);
    void submit(const HttpRequestPtr& req, Callback&& cb);
    void hint(const HttpRequestPtr& req, Callback&& cb);
    void history(const HttpRequestPtr& req, Callback&& cb);
    void continueLearning(const HttpRequestPtr& req, Callback&& cb);
    void aiNextTask(const HttpRequestPtr& req, Callback&& cb);
    void aiExplain(const HttpRequestPtr& req, Callback&& cb);
    void aiTips(const HttpRequestPtr& req, Callback&& cb);
    void aiForecast(const HttpRequestPtr& req, Callback&& cb);
    void aiMastery(const HttpRequestPtr& req, Callback&& cb);
    void aiGenerate(const HttpRequestPtr& req, Callback&& cb);
};

} // namespace lumina

