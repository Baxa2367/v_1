#include "http/controllers/staff_controller.h"
#include "services/admin_service.h"

namespace lumina {

using namespace drogon;

namespace {
AuthCtx ctxOf(const HttpRequestPtr& req) {
    auto ctx = http::authCtx(req);
    ctx.locale = http::localeOf(req, &ctx);
    return ctx;
}
} // namespace

void StaffController::courses(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeacherService::courses(ctxOf(req)); });
}
void StaffController::createCourse(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeacherService::createCourse(ctxOf(req), http::parseBody(req)); });
}
void StaffController::updateCourse(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return TeacherService::updateCourse(ctxOf(req), http::pathParam(req, "id"), http::parseBody(req));
    });
}
void StaffController::deleteCourse(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeacherService::deleteCourse(ctxOf(req), http::pathParam(req, "id")); });
}
void StaffController::analytics(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeacherService::analytics(ctxOf(req), http::pathParam(req, "id")); });
}
void StaffController::topics(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeacherService::topics(ctxOf(req), http::pathParam(req, "id")); });
}
void StaffController::createTopic(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeacherService::createTopic(ctxOf(req), http::parseBody(req)); });
}
void StaffController::createLevel(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeacherService::createLevel(ctxOf(req), http::parseBody(req)); });
}
void StaffController::updateLevel(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return TeacherService::updateLevel(ctxOf(req), http::pathParam(req, "id"), http::parseBody(req));
    });
}
void StaffController::deleteLevel(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeacherService::deleteLevel(ctxOf(req), http::pathParam(req, "id")); });
}
void StaffController::createTask(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeacherService::createTask(ctxOf(req), http::parseBody(req)); });
}
void StaffController::importTasks(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeacherService::importTasks(ctxOf(req), http::parseBody(req)); });
}
void StaffController::updateTask(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return TeacherService::updateTask(ctxOf(req), http::pathParam(req, "id"), http::parseBody(req));
    });
}
void StaffController::deleteTask(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeacherService::deleteTask(ctxOf(req), http::pathParam(req, "id")); });
}
void StaffController::users(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        int limit = 50, offset = 0;
        try {
            limit = std::stoi(req->getParameter("limit"));
            offset = std::stoi(req->getParameter("offset"));
        } catch (...) {
        }
        return AdminService::users(ctxOf(req), req->getParameter("q"), limit, offset);
    });
}
void StaffController::updateUser(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return AdminService::updateUser(ctxOf(req), http::pathParam(req, "id"), http::parseBody(req));
    });
}
void StaffController::anticheat(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        int limit = 50;
        try {
            limit = std::stoi(req->getParameter("limit"));
        } catch (...) {
        }
        return AdminService::anticheat(ctxOf(req), req->getParameter("status"), limit);
    });
}
void StaffController::resolveFlag(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return AdminService::resolveFlag(ctxOf(req), http::pathParam(req, "id"), http::parseBody(req));
    });
}
void StaffController::seasons(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return AdminService::seasons(ctxOf(req)); });
}
void StaffController::createSeason(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return AdminService::createSeason(ctxOf(req), http::parseBody(req)); });
}
void StaffController::updateSeason(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return AdminService::updateSeason(ctxOf(req), http::pathParam(req, "id"), http::parseBody(req));
    });
}
void StaffController::closeSeason(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return AdminService::closeSeason(ctxOf(req), http::pathParam(req, "id")); });
}
void StaffController::createAchievement(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return AdminService::createAchievement(ctxOf(req), http::parseBody(req)); });
}
void StaffController::metrics(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return AdminService::metrics(ctxOf(req)); });
}
void StaffController::audit(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        int limit = 50;
        try {
            limit = std::stoi(req->getParameter("limit"));
        } catch (...) {
        }
        return AdminService::audit(ctxOf(req), limit);
    });
}
void StaffController::economy(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return AdminService::economy(ctxOf(req), http::parseBody(req)); });
}

} // namespace lumina


extern "C" void lumina_link_staff_controller() {}


