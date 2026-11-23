#include "http/controllers/learning_controller.h"
#include "core/i18n.h"
#include "services/ai_service.h"
#include "services/attempt_service.h"
#include "services/course_service.h"
#include "services/xp_engine.h"
#include "ws/ws_hub.h"

namespace lumina {

using namespace drogon;

namespace {
AuthCtx ctxOf(const HttpRequestPtr& req) {
    auto ctx = http::authCtx(req);
    ctx.locale = http::localeOf(req, &ctx);
    return ctx;
}
} // namespace

void LearningController::courses(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        const std::string category = req->getParameter("category");
        const std::string search = req->getParameter("q");
        const bool enrolledOnly = req->getParameter("enrolled") == "1";
        return CourseService::listCourses(ctxOf(req), category, search, enrolledOnly);
    });
}

void LearningController::course(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return CourseService::getCourse(ctxOf(req), http::pathParam(req, "id")); });
}

void LearningController::map(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return CourseService::courseMap(ctxOf(req), http::pathParam(req, "id")); });
}

void LearningController::enroll(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return CourseService::enroll(ctxOf(req), http::pathParam(req, "id")); });
}

void LearningController::unenroll(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return CourseService::unenroll(ctxOf(req), http::pathParam(req, "id")); });
}

void LearningController::level(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return CourseService::levelDetail(ctxOf(req), http::pathParam(req, "id")); });
}

void LearningController::nextTask(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return CourseService::nextTask(ctxOf(req), http::pathParam(req, "id"),
                                       req->getParameter("exclude"));
    });
}

void LearningController::task(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return AttemptService::taskForStudent(ctxOf(req), http::pathParam(req, "id"));
    });
}

void LearningController::submit(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        auto ctx = ctxOf(req);
        auto result = AttemptService::submit(ctx, http::pathParam(req, "id"), http::parseBody(req),
                                             http::clientIp(req));

        Hub::instance().send(ctx.userId,
                             json{{"type", "xp_gained"},
                                  {"amount", result["xp"]["amount"]},
                                  {"total_xp", result["xp"]["total_xp"]},
                                  {"level", result["xp"]["level"]},
                                  {"combo", result["combo"]}});
        if (result["xp"].value("level_up", false)) {
            Hub::instance().send(ctx.userId,
                                 json{{"type", "level_up"},
                                      {"level", result["xp"]["level"]},
                                      {"title", result["xp"]["title"]}});
        }
        for (const auto& a : result["achievements"]) {
            Hub::instance().send(ctx.userId,
                                 json{{"type", "achievement_unlocked"},
                                      {"code", a["code"]},
                                      {"title", a["title"]},
                                      {"icon", a["icon"]},
                                      {"rarity", a["rarity"]}});
        }
        return result;
    });
}

void LearningController::hint(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return AttemptService::hint(ctxOf(req), http::pathParam(req, "id"), http::parseBody(req));
    });
}

void LearningController::history(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        int limit = 20, offset = 0;
        try {
            limit = std::stoi(req->getParameter("limit"));
            offset = std::stoi(req->getParameter("offset"));
        } catch (...) {
        }
        return AttemptService::history(ctxOf(req), limit, offset);
    });
}

void LearningController::continueLearning(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return CourseService::continueLearning(ctxOf(req)); });
}

void LearningController::aiNextTask(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return AiService::nextTask(ctxOf(req), req->getParameter("course_id"));
    });
}

void LearningController::aiExplain(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return AiService::explain(ctxOf(req), http::pathParam(req, "id")); });
}

void LearningController::aiTips(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return AiService::tips(ctxOf(req)); });
}

void LearningController::aiForecast(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return AiService::forecast(ctxOf(req)); });
}

void LearningController::aiMastery(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return AiService::mastery(ctxOf(req), req->getParameter("course_id"));
    });
}

void LearningController::aiGenerate(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return AiService::generateTask(ctxOf(req), http::parseBody(req)); });
}

} // namespace lumina

extern "C" void lumina_link_learning_controller() {}


