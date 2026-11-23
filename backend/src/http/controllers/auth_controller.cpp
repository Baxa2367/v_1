#include "http/controllers/auth_controller.h"
#include "core/i18n.h"
#include "services/achievement_service.h"
#include "services/ai_service.h"
#include "services/auth_service.h"
#include "services/course_service.h"
#include "services/notification_service.h"
#include "services/quest_service.h"
#include "services/xp_engine.h"
#include "services/common.h"

namespace lumina {

using namespace drogon;

void AuthController::registerUser(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return AuthService::registerUser(http::parseBody(req), http::clientIp(req),
                                         req->getHeader("user-agent"));
    });
}

void AuthController::login(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return AuthService::login(http::parseBody(req), http::clientIp(req),
                                  req->getHeader("user-agent"));
    });
}

void AuthController::refresh(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return AuthService::refresh(http::parseBody(req), http::clientIp(req),
                                    req->getHeader("user-agent"));
    });
}

void AuthController::logout(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        AuthService::logout(http::parseBody(req));
        return json{{"ok", true}};
    });
}

void AuthController::me(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        auto ctx = http::authCtx(req);
        ctx.locale = http::localeOf(req, &ctx);
        return AuthService::me(ctx);
    });
}

void AuthController::updateMe(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        auto ctx = http::authCtx(req);
        ctx.locale = http::localeOf(req, &ctx);
        return AuthService::updateProfile(ctx, http::parseBody(req));
    });
}

void AuthController::changePassword(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        auto ctx = http::authCtx(req);
        return AuthService::changePassword(ctx, http::parseBody(req));
    });
}

void AuthController::dashboard(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        auto ctx = http::authCtx(req);
        ctx.locale = http::localeOf(req, &ctx);

        json out;
        auto profile = Db::instance().tx([&](Tx& t) { return svc::loadStudent(t, ctx.userId, ctx.locale); });
        out["profile"] = profile;

        auto today = Db::instance().tx([&](Tx& t) {
            auto goal = t.one(
                "SELECT goal_xp, xp_earned, completed_at FROM daily_goals WHERE student_id = $1 "
                "AND goal_date = (now() AT TIME ZONE 'Asia/Qyzylorda')::date",
                {param(ctx.userId)});
            auto todayXp = t.scalarInt(
                "SELECT coalesce(sum(amount), 0) FROM xp_events WHERE student_id = $1 "
                "AND amount > 0 AND (created_at AT TIME ZONE 'Asia/Qyzylorda')::date = "
                "(now() AT TIME ZONE 'Asia/Qyzylorda')::date",
                {param(ctx.userId)});
            return json{{"goal_xp", goal.is_null() ? profile.value("daily_goal_xp", 50) : goal.value("goal_xp", 50)},
                        {"xp_earned", todayXp},
                        {"completed", !goal.is_null() && goal.value("xp_earned", 0) >= goal.value("goal_xp", 50)},
                        {"goal_row", goal}};
        });
        out["today"] = today;

        auto rank = Db::instance().tx([&](Tx& t) {
            const long long xp = profile.value("total_xp", 0LL);
            const long long ahead = t.scalarInt(
                "SELECT count(*) FROM students s JOIN users u ON u.id = s.user_id "
                "WHERE u.status = 'active' AND s.total_xp > $1", {param(xp)});
            const long long weekly = t.scalarInt(
                "SELECT coalesce(sum(amount), 0) FROM xp_events WHERE student_id = $1 AND amount > 0 "
                "AND created_at >= date_trunc('week', now())", {param(ctx.userId)});
            const long long weeklyAhead = t.scalarInt(
                "SELECT count(*) FROM (SELECT student_id, sum(amount) x FROM xp_events "
                "WHERE amount > 0 AND created_at >= date_trunc('week', now()) GROUP BY student_id) s "
                "WHERE s.x > $1", {param(weekly)});
            return json{{"global", ahead + 1}, {"weekly", weeklyAhead + 1}, {"weekly_xp", weekly}};
        });
        out["rank"] = rank;

        out["continue"] = CourseService::continueLearning(ctx);
        out["quests"] = QuestService::list(ctx);
        out["streak"] = {{"current", profile.value("current_streak", 0)},
                         {"longest", profile.value("longest_streak", 0)},
                         {"xp_today", today.value("xp_earned", 0)}};

        auto recent = Db::instance().tx([&](Tx& t) {
            auto rows = t.query(
                "SELECT a.code, a.title, a.icon, a.rarity, sa.awarded_at FROM student_achievements sa "
                "JOIN achievements a ON a.id = sa.achievement_id WHERE sa.student_id = $1 "
                "ORDER BY sa.awarded_at DESC LIMIT 3",
                {param(ctx.userId)});
            json arr = json::array();
            for (const auto& r : rows) {
                json item = r;
                item["title"] = I18n::pick(r["title"], ctx.locale);
                arr.push_back(item);
            }
            return arr;
        });
        out["recent_achievements"] = recent;

        auto latestTip = Db::instance().tx([&](Tx& t) {
            return t.one("SELECT kind, text, created_at FROM ai_tips WHERE student_id = $1 "
                         "ORDER BY created_at DESC LIMIT 1",
                         {param(ctx.userId)});
        });
        if (latestTip.is_null()) {
            auto tips = AiService::tips(ctx);
            out["tip"] = tips["tips"].empty() ? json(nullptr) : tips["tips"][0];
        } else {
            latestTip["text"] = I18n::pick(latestTip["text"], ctx.locale);
            out["tip"] = latestTip;
        }
        out["active_season"] = Db::instance().tx([&](Tx& t) {
            return t.one("SELECT id, code, title, ends_at FROM seasons WHERE status = 'active' LIMIT 1");
        });
        return out;
    });
}

void AuthController::notifications(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        auto ctx = http::authCtx(req);
        ctx.locale = http::localeOf(req, &ctx);
        int limit = 30;
        try {
            limit = std::stoi(req->getParameter("limit"));
        } catch (...) {
        }
        return NotificationService::list(ctx, limit);
    });
}

void AuthController::readAll(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        auto ctx = http::authCtx(req);
        NotificationService::markAllRead(ctx);
        return json{{"ok", true}};
    });
}

void AuthController::readOne(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        auto ctx = http::authCtx(req);
        NotificationService::markRead(ctx, http::pathParam(req, "id"));
        return json{{"ok", true}};
    });
}

} // namespace lumina

extern "C" void lumina_link_auth_controller() {}


