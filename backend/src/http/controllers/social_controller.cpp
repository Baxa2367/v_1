#include "http/controllers/social_controller.h"
#include "core/i18n.h"
#include "services/achievement_service.h"
#include "services/leaderboard_service.h"
#include "services/quest_service.h"
#include "services/report_service.h"
#include "services/season_service.h"
#include "services/shop_service.h"
#include "services/team_service.h"
#include "services/xp_engine.h"

namespace lumina {

using namespace drogon;

namespace {
AuthCtx ctxOf(const HttpRequestPtr& req) {
    auto ctx = http::authCtx(req);
    ctx.locale = http::localeOf(req, &ctx);
    return ctx;
}
int intParam(const HttpRequestPtr& req, const std::string& name, int def) {
    try {
        const std::string v = req->getParameter(name);
        return v.empty() ? def : std::stoi(v);
    } catch (...) {
        return def;
    }
}
} // namespace

void SocialController::leaderboard(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        std::string scope = req->getParameter("scope");
        if (scope.empty()) scope = "global";
        auto season = req->getParameter("season_id");
        if (scope == "season" && season.empty()) {
            season = Db::instance().tx([&](Tx& t) {
                return t.scalar("SELECT id::text FROM seasons WHERE status = 'active' LIMIT 1")
                    .value_or("");
            });
        }
        return LeaderboardService::query(ctxOf(req), scope, req->getParameter("period"),
                                         req->getParameter("course_id"), season,
                                         intParam(req, "limit", 20), intParam(req, "offset", 0));
    });
}

void SocialController::leaderboardMe(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        std::string scope = req->getParameter("scope");
        if (scope.empty()) scope = "global";
        auto season = req->getParameter("season_id");
        if (scope == "season" && season.empty()) {
            season = Db::instance().tx([&](Tx& t) {
                return t.scalar("SELECT id::text FROM seasons WHERE status = 'active' LIMIT 1")
                    .value_or("");
            });
        }
        return LeaderboardService::myPosition(ctxOf(req), scope, req->getParameter("course_id"),
                                              season);
    });
}

void SocialController::shop(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return ShopService::list(ctxOf(req)); });
}

void SocialController::buy(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return ShopService::buy(ctxOf(req), http::pathParam(req, "id")); });
}

void SocialController::inventory(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return ShopService::inventory(ctxOf(req)); });
}

void SocialController::equip(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        bool equipped = true;
        try {
            equipped = http::parseBody(req).value("equipped", true);
        } catch (...) {
        }
        return ShopService::equip(ctxOf(req), http::pathParam(req, "id"), equipped);
    });
}

void SocialController::seasons(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return SeasonService::list(ctxOf(req)); });
}

void SocialController::currentSeason(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return SeasonService::current(ctxOf(req)); });
}

void SocialController::seasonRewards(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return SeasonService::rewards(ctxOf(req), http::pathParam(req, "id")); });
}

void SocialController::claimSeason(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        return SeasonService::claim(ctxOf(req), http::pathParam(req, "id"),
                                    intParam(req, "tier", 0));
    });
}

void SocialController::teams(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeamService::list(ctxOf(req)); });
}

void SocialController::myTeam(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeamService::mine(ctxOf(req)); });
}

void SocialController::createTeam(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeamService::create(ctxOf(req), http::parseBody(req)); });
}

void SocialController::joinTeam(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeamService::join(ctxOf(req), http::parseBody(req)); });
}

void SocialController::leaveTeam(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeamService::leave(ctxOf(req)); });
}

void SocialController::team(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeamService::detail(ctxOf(req), http::pathParam(req, "id")); });
}

void SocialController::tournaments(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeamService::tournaments(ctxOf(req)); });
}

void SocialController::standings(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return TeamService::standings(ctxOf(req), http::pathParam(req, "id")); });
}

void SocialController::reportProgress(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return ReportService::progress(ctxOf(req)); });
}

void SocialController::reportXp(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return ReportService::xp(ctxOf(req), intParam(req, "days", 30)); });
}

void SocialController::reportTopics(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return ReportService::topics(ctxOf(req)); });
}

void SocialController::reportAchievements(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return ReportService::achievements(ctxOf(req)); });
}

void SocialController::reportLeaderboard(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return ReportService::leaderboard(ctxOf(req)); });
}

void SocialController::reportHeatmap(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return ReportService::heatmap(ctxOf(req), intParam(req, "days", 120)); });
}

void SocialController::reportOverview(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return ReportService::overview(ctxOf(req)); });
}

void SocialController::exportReport(const HttpRequestPtr& req, Callback&& cb) {
    try {
        const std::string type = req->getParameter("type").empty() ? "progress"
                                                                   : req->getParameter("type");
        const std::string format = req->getParameter("format").empty() ? "csv"
                                                                      : req->getParameter("format");
        auto file = ReportService::exportReport(ctxOf(req), type, format);
        cb(http::binary(std::move(file.body), file.contentType, file.filename));
    } catch (const ApiError& e) {
        cb(http::fail(e));
    } catch (const std::exception& e) {
        cb(http::fail(500, "internal_error", e.what()));
    }
}

void SocialController::myXp(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        auto ctx = ctxOf(req);
        return Db::instance().tx([&](Tx& t) {
            auto s = t.one("SELECT total_xp, level, coins, gems FROM students WHERE user_id = $1",
                           {param(ctx.userId)});
            if (s.is_null()) throw notFound("Student not found");
            const long long total = s.value("total_xp", 0LL);
            auto progress = svc::xpProgress(total, ctx.locale);
            auto today = t.scalarInt(
                "SELECT coalesce(sum(amount),0) FROM xp_events WHERE student_id = $1 AND amount > 0 "
                "AND (created_at AT TIME ZONE 'Asia/Qyzylorda')::date = "
                "(now() AT TIME ZONE 'Asia/Qyzylorda')::date",
                {param(ctx.userId)});
            auto week = t.scalarInt(
                "SELECT coalesce(sum(amount),0) FROM xp_events WHERE student_id = $1 AND amount > 0 "
                "AND created_at >= date_trunc('week', now())",
                {param(ctx.userId)});
            progress["today"] = today;
            progress["week"] = week;
            progress["coins"] = s.value("coins", 0);
            return progress;
        });
    });
}

void SocialController::myXpEvents(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        auto ctx = ctxOf(req);
        const int limit = std::clamp(intParam(req, "limit", 30), 1, 200);
        return Db::instance().tx([&](Tx& t) {
            return json{{"events", t.query(
                                     "SELECT id, amount, reason, source_type, source_id, balance_after, "
                                     "created_at FROM xp_events WHERE student_id = $1 "
                                     "ORDER BY created_at DESC LIMIT $2",
                                     {param(ctx.userId), param(limit)})}};
        });
    });
}

void SocialController::myAchievements(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return AchievementService::collection(ctxOf(req)); });
}

void SocialController::myStreak(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] {
        auto ctx = ctxOf(req);
        return Db::instance().tx([&](Tx& t) {
            auto s = t.one("SELECT current_streak, longest_streak, last_activity_date, "
                           "daily_goal_xp FROM students WHERE user_id = $1",
                           {param(ctx.userId)});
            auto calendar = t.query(
                "SELECT d::date::text AS day, coalesce(sum(e.amount),0) AS xp, "
                "coalesce(max(sl.goal_met), false) AS goal_met "
                "FROM generate_series(current_date - 89, current_date, interval '1 day') d "
                "LEFT JOIN xp_events e ON e.student_id = $1 "
                "AND (e.created_at AT TIME ZONE 'Asia/Qyzylorda')::date = d::date "
                "LEFT JOIN streak_log sl ON sl.student_id = $1 AND sl.log_date = d::date "
                "GROUP BY d ORDER BY d",
                {param(ctx.userId)});
            auto freezes = t.scalarInt(
                "SELECT coalesce(sum(i.qty),0) FROM inventory i JOIN shop_items s ON s.id = i.item_id "
                "WHERE i.student_id = $1 AND s.kind = 'streak_freeze'",
                {param(ctx.userId)});
            return json{{"current", s.is_null() ? 0 : s.value("current_streak", 0)},
                        {"longest", s.is_null() ? 0 : s.value("longest_streak", 0)},
                        {"last_activity_date", s.is_null() ? json(nullptr) : s["last_activity_date"]},
                        {"daily_goal_xp", s.is_null() ? 50 : s.value("daily_goal_xp", 50)},
                        {"freezes", freezes},
                        {"calendar", calendar}};
        });
    });
}

void SocialController::myQuests(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return QuestService::list(ctxOf(req)); });
}

void SocialController::claimQuest(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return QuestService::claim(ctxOf(req), http::pathParam(req, "id")); });
}

void SocialController::myHeatmap(const HttpRequestPtr& req, Callback&& cb) {
    http::run(cb, [&] { return ReportService::heatmap(ctxOf(req), intParam(req, "days", 120)); });
}

} // namespace lumina

extern "C" void lumina_link_social_controller() {}



