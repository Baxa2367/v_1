#include "services/leaderboard_service.h"
#include "core/i18n.h"
#include "core/util.h"
#include "services/xp_engine.h"

#include <algorithm>

namespace lumina {

namespace {

struct Board {
    std::string scoreExpr;
    std::string fromJoin;
    Params params;           // $1.. for course/season filters
    std::string where = " u.status = 'active' AND "
                        "coalesce((s.privacy_flags->>'public_leaderboard')::boolean, true) ";
};

Board buildBoard(const std::string& scope, const std::string& courseId,
                 const std::string& seasonId) {
    Board b;
    if (scope == "weekly") {
        b.scoreExpr = "coalesce(agg.xp, 0)";
        b.fromJoin =
            "FROM students s JOIN users u ON u.id = s.user_id "
            "LEFT JOIN (SELECT student_id, sum(amount) AS xp FROM xp_events "
            "WHERE amount > 0 AND created_at >= date_trunc('week', now()) "
            "GROUP BY student_id) agg ON agg.student_id = s.user_id ";
    } else if (scope == "course") {
        b.scoreExpr = "coalesce(agg.xp, 0)";
        b.fromJoin =
            "FROM students s JOIN users u ON u.id = s.user_id "
            "LEFT JOIN (SELECT student_id, sum(amount) AS xp FROM xp_events "
            "WHERE amount > 0 AND course_id = $1 GROUP BY student_id) agg "
            "ON agg.student_id = s.user_id ";
        b.params.push_back(param(courseId));
    } else if (scope == "season") {
        b.scoreExpr = "coalesce(sp.season_xp, 0)";
        b.fromJoin =
            "FROM students s JOIN users u ON u.id = s.user_id "
            "LEFT JOIN season_progress sp ON sp.student_id = s.user_id AND sp.season_id = $1 ";
        b.params.push_back(param(seasonId));
        b.where += " AND s.last_active_at > now() - interval '180 days' ";
    } else {
        b.scoreExpr = "s.total_xp";
        b.fromJoin = "FROM students s JOIN users u ON u.id = s.user_id ";
    }
    return b;
}

std::string rankedSql(const Board& b) {
    return "WITH ranked AS (SELECT s.user_id, s.nickname, s.avatar, s.level, s.league, "
           + b.scoreExpr + " AS score, row_number() OVER (ORDER BY " + b.scoreExpr +
           " DESC, s.user_id) AS rank " + b.fromJoin + " WHERE " + b.where + ") ";
}

json previousRanks(const std::string& scope) {
    json out = json::object();
    auto rows = Db::instance().query(
        "SELECT entity_id, rank FROM leaderboards WHERE scope = $1 AND snapshot_at::date = "
        "(SELECT max(snapshot_at)::date FROM leaderboards WHERE scope = $1) "
        "AND snapshot_at::date < current_date",
        {param(scope)});
    for (const auto& r : rows) out[r["entity_id"].get<std::string>()] = r["rank"].get<int>();
    return out;
}

} // namespace

json LeaderboardService::query(const AuthCtx& ctx, const std::string& scope,
                               const std::string& period, const std::string& courseId,
                               const std::string& seasonId, int limit, int offset) {
    limit = std::clamp(limit, 1, 100);
    offset = std::max(0, offset);
    if (scope == "team") return teamStandings(ctx, period, limit);

    return Db::instance().tx([&](Tx& t) {
        Board b = buildBoard(scope, courseId, seasonId);
        Params params = b.params;
        params.push_back(param(limit));
        params.push_back(param(offset));
        const std::string sql = rankedSql(b) + "SELECT * FROM ranked ORDER BY rank LIMIT $" +
                                std::to_string(b.params.size() + 1) + " OFFSET $" +
                                std::to_string(b.params.size() + 2);
        auto rows = t.query(sql, params);
        auto prev = previousRanks(scope);

        json entries = json::array();
        int myRank = 0;
        double myScore = 0;
        for (const auto& r : rows) {
            const std::string id = r["user_id"].get<std::string>();
            const int rank = r.value("rank", 0);
            int delta = 0;
            if (prev.contains(id)) delta = prev[id].get<int>() - rank;
            const bool isMe = id == ctx.userId;
            if (isMe) {
                myRank = rank;
                myScore = r.value("score", 0.0);
            }
            entries.push_back({{"rank", rank},
                               {"student_id", id},
                               {"nickname", r["nickname"]},
                               {"avatar", r["avatar"]},
                               {"level", r.value("level", 1)},
                               {"title", XpEngine::titleForLevel(r.value("level", 1), ctx.locale)},
                               {"league", r["league"]},
                               {"score", r.value("score", 0.0)},
                               {"delta", delta},
                               {"is_me", isMe}});
        }

        if (myRank == 0) {
            Params mineParams = b.params;
            mineParams.push_back(param(ctx.userId));
            auto mine = t.one(rankedSql(b) + "SELECT rank, score FROM ranked WHERE user_id = $" +
                                  std::to_string(b.params.size() + 1),
                              mineParams);
            if (!mine.is_null()) {
                myRank = mine.value("rank", 0);
                myScore = mine.value("score", 0.0);
            }
        }

        const int64_t total = t.scalarInt(
            "SELECT count(*) FROM students s JOIN users u ON u.id = s.user_id WHERE u.status = 'active'");
        return json{{"scope", scope},
                    {"period", period},
                    {"entries", entries},
                    {"total", total},
                    {"me", {{"rank", myRank}, {"score", myScore}}}};
    });
}

json LeaderboardService::myPosition(const AuthCtx& ctx, const std::string& scope,
                                    const std::string& courseId, const std::string& seasonId) {
    auto board = query(ctx, scope, "current", courseId, seasonId, 1, 0);
    return board["me"];
}

void LeaderboardService::snapshot(const std::string& scope, const std::string& period,
                                  const std::string& seasonId, const std::string& courseId) {
    Db::instance().tx([&](Tx& t) {
        Board b = buildBoard(scope, courseId, seasonId);
        const size_t n = b.params.size();
        Params params = b.params;
        params.push_back(param(scope));
        params.push_back(param(period));
        params.push_back(seasonId.empty() ? std::nullopt : param(seasonId));
        params.push_back(courseId.empty() ? std::nullopt : param(courseId));
        const std::string sql =
            rankedSql(b) +
            "INSERT INTO leaderboards (scope, period, season_id, course_id, entity_type, "
            "entity_id, score, rank, snapshot_at) SELECT $" + std::to_string(n + 1) + ", $" +
            std::to_string(n + 2) + ", $" + std::to_string(n + 3) + ", $" +
            std::to_string(n + 4) + ", 'student', user_id, score, rank, now() FROM ranked";
        t.exec(sql, params);
    });
}

json LeaderboardService::teamStandings(const AuthCtx& ctx, const std::string& period, int limit) {
    (void)ctx;
    return Db::instance().tx([&](Tx& t) {
        const bool weekly = period == "week";
        const std::string xpExpr =
            weekly ? "coalesce((SELECT sum(e.amount) FROM xp_events e JOIN team_members tm2 "
                     "ON tm2.student_id = e.student_id WHERE tm2.team_id = tm.id AND e.amount > 0 "
                     "AND e.created_at >= date_trunc('week', now())), 0)"
                   : "coalesce((SELECT sum(s.total_xp) FROM team_members tm2 "
                     "JOIN students s ON s.user_id = tm2.student_id WHERE tm2.team_id = tm.id), 0)";
        auto rows = t.query(
            "SELECT tm.team_id, t.name, t.emblem, " + xpExpr +
                " AS score, (SELECT count(*) FROM team_members x WHERE x.team_id = tm.team_id) "
                "AS members FROM (SELECT DISTINCT team_id FROM team_members) tm "
                "JOIN teams t ON t.id = tm.team_id ORDER BY score DESC LIMIT $1",
            {param(limit)});
        json entries = json::array();
        int rank = 1;
        for (const auto& r : rows) {
            entries.push_back({{"rank", rank++},
                               {"team_id", r["team_id"]},
                               {"name", r["name"]},
                               {"emblem", r["emblem"]},
                               {"score", r.value("score", 0.0)},
                               {"members", r.value("members", 0)}});
        }
        return json{{"scope", "team"}, {"entries", entries}, {"total", entries.size()}};
    });
}

} // namespace lumina
