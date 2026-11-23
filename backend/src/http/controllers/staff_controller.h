#pragma once
#include "http/http_utils.h"
#include <drogon/HttpController.h>

namespace lumina {

using http::Callback;
using http::HttpRequestPtr;

class StaffController : public drogon::HttpController<StaffController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(StaffController::courses, "/api/v1/teacher/courses", drogon::Get,
                  "lumina::AuthFilter", "lumina::TeacherFilter");
    ADD_METHOD_TO(StaffController::createCourse, "/api/v1/teacher/courses", drogon::Post,
                  "lumina::AuthFilter", "lumina::TeacherFilter");
    ADD_METHOD_TO(StaffController::updateCourse, "/api/v1/teacher/courses/{id}", drogon::Put,
                  "lumina::AuthFilter", "lumina::TeacherFilter");
    ADD_METHOD_TO(StaffController::deleteCourse, "/api/v1/teacher/courses/{id}", drogon::Delete,
                  "lumina::AuthFilter", "lumina::TeacherFilter");
    ADD_METHOD_TO(StaffController::analytics, "/api/v1/teacher/courses/{id}/analytics", drogon::Get,
                  "lumina::AuthFilter", "lumina::TeacherFilter");
    ADD_METHOD_TO(StaffController::topics, "/api/v1/teacher/courses/{id}/topics", drogon::Get,
                  "lumina::AuthFilter", "lumina::TeacherFilter");
    ADD_METHOD_TO(StaffController::createTopic, "/api/v1/teacher/topics", drogon::Post,
                  "lumina::AuthFilter", "lumina::TeacherFilter");
    ADD_METHOD_TO(StaffController::createLevel, "/api/v1/teacher/levels", drogon::Post,
                  "lumina::AuthFilter", "lumina::TeacherFilter");
    ADD_METHOD_TO(StaffController::updateLevel, "/api/v1/teacher/levels/{id}", drogon::Put,
                  "lumina::AuthFilter", "lumina::TeacherFilter");
    ADD_METHOD_TO(StaffController::deleteLevel, "/api/v1/teacher/levels/{id}", drogon::Delete,
                  "lumina::AuthFilter", "lumina::TeacherFilter");
    ADD_METHOD_TO(StaffController::createTask, "/api/v1/teacher/tasks", drogon::Post,
                  "lumina::AuthFilter", "lumina::TeacherFilter");
    ADD_METHOD_TO(StaffController::importTasks, "/api/v1/teacher/tasks/import", drogon::Post,
                  "lumina::AuthFilter", "lumina::TeacherFilter");
    ADD_METHOD_TO(StaffController::updateTask, "/api/v1/teacher/tasks/{id}", drogon::Put,
                  "lumina::AuthFilter", "lumina::TeacherFilter");
    ADD_METHOD_TO(StaffController::deleteTask, "/api/v1/teacher/tasks/{id}", drogon::Delete,
                  "lumina::AuthFilter", "lumina::TeacherFilter");

    ADD_METHOD_TO(StaffController::users, "/api/v1/admin/users", drogon::Get,
                  "lumina::AuthFilter", "lumina::AdminFilter");
    ADD_METHOD_TO(StaffController::updateUser, "/api/v1/admin/users/{id}", drogon::Patch,
                  "lumina::AuthFilter", "lumina::AdminFilter");
    ADD_METHOD_TO(StaffController::anticheat, "/api/v1/admin/anticheat", drogon::Get,
                  "lumina::AuthFilter", "lumina::AdminFilter");
    ADD_METHOD_TO(StaffController::resolveFlag, "/api/v1/admin/anticheat/{id}", drogon::Patch,
                  "lumina::AuthFilter", "lumina::AdminFilter");
    ADD_METHOD_TO(StaffController::seasons, "/api/v1/admin/seasons", drogon::Get,
                  "lumina::AuthFilter", "lumina::AdminFilter");
    ADD_METHOD_TO(StaffController::createSeason, "/api/v1/admin/seasons", drogon::Post,
                  "lumina::AuthFilter", "lumina::AdminFilter");
    ADD_METHOD_TO(StaffController::updateSeason, "/api/v1/admin/seasons/{id}", drogon::Put,
                  "lumina::AuthFilter", "lumina::AdminFilter");
    ADD_METHOD_TO(StaffController::closeSeason, "/api/v1/admin/seasons/{id}/close", drogon::Post,
                  "lumina::AuthFilter", "lumina::AdminFilter");
    ADD_METHOD_TO(StaffController::createAchievement, "/api/v1/admin/achievements", drogon::Post,
                  "lumina::AuthFilter", "lumina::AdminFilter");
    ADD_METHOD_TO(StaffController::metrics, "/api/v1/admin/metrics", drogon::Get,
                  "lumina::AuthFilter", "lumina::AdminFilter");
    ADD_METHOD_TO(StaffController::audit, "/api/v1/admin/audit", drogon::Get,
                  "lumina::AuthFilter", "lumina::AdminFilter");
    ADD_METHOD_TO(StaffController::economy, "/api/v1/admin/economy", drogon::Get,
                  "lumina::AuthFilter", "lumina::AdminFilter");
    METHOD_LIST_END

    void courses(const HttpRequestPtr& req, Callback&& cb);
    void createCourse(const HttpRequestPtr& req, Callback&& cb);
    void updateCourse(const HttpRequestPtr& req, Callback&& cb);
    void deleteCourse(const HttpRequestPtr& req, Callback&& cb);
    void analytics(const HttpRequestPtr& req, Callback&& cb);
    void topics(const HttpRequestPtr& req, Callback&& cb);
    void createTopic(const HttpRequestPtr& req, Callback&& cb);
    void createLevel(const HttpRequestPtr& req, Callback&& cb);
    void updateLevel(const HttpRequestPtr& req, Callback&& cb);
    void deleteLevel(const HttpRequestPtr& req, Callback&& cb);
    void createTask(const HttpRequestPtr& req, Callback&& cb);
    void importTasks(const HttpRequestPtr& req, Callback&& cb);
    void updateTask(const HttpRequestPtr& req, Callback&& cb);
    void deleteTask(const HttpRequestPtr& req, Callback&& cb);
    void users(const HttpRequestPtr& req, Callback&& cb);
    void updateUser(const HttpRequestPtr& req, Callback&& cb);
    void anticheat(const HttpRequestPtr& req, Callback&& cb);
    void resolveFlag(const HttpRequestPtr& req, Callback&& cb);
    void seasons(const HttpRequestPtr& req, Callback&& cb);
    void createSeason(const HttpRequestPtr& req, Callback&& cb);
    void updateSeason(const HttpRequestPtr& req, Callback&& cb);
    void closeSeason(const HttpRequestPtr& req, Callback&& cb);
    void createAchievement(const HttpRequestPtr& req, Callback&& cb);
    void metrics(const HttpRequestPtr& req, Callback&& cb);
    void audit(const HttpRequestPtr& req, Callback&& cb);
    void economy(const HttpRequestPtr& req, Callback&& cb);
};

} // namespace lumina

