#include "jobs/worker.h"
#include "core/log.h"
#include "db/db.h"
#include "services/leaderboard_service.h"
#include "services/season_service.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace lumina::jobs {

namespace {
std::thread g_thread;
std::atomic_bool g_stop{false};
std::mutex g_onceMutex;
std::string g_lastDaily;
std::string g_lastWeekly;

std::string today() {
    return Db::instance().tx([](Tx& t) {
        return t.scalar("SELECT (now() AT TIME ZONE 'Asia/Qyzylorda')::date::text").value_or("");
    });
}

void activateSeasons() {
    Db::instance().tx([](Tx& t) {
        t.exec("UPDATE seasons SET status = 'active' WHERE status = 'upcoming' "
               "AND now() >= starts_at AND now() < ends_at");
    });
}

void closeFinishedSeasons() {
    auto rows = Db::instance().query(
        "SELECT id FROM seasons WHERE status = 'active' AND ends_at < now()");
    for (const auto& r : rows) {
        try {
            SeasonService::closeSeason(r["id"].get<std::string>());
        } catch (const std::exception& e) {
            Log::get()->error("season close failed: {}", e.what());
        }
    }
}

void dailySnapshots() {
    const std::string d = today();
    if (d.empty() || d == g_lastDaily) return;
    try {
        LeaderboardService::snapshot("global", "all", "", "");
        LeaderboardService::snapshot("weekly", "week", "", "");
        g_lastDaily = d;
        Log::get()->info("leaderboard snapshots written for {}", d);
    } catch (const std::exception& e) {
        Log::get()->error("snapshot failed: {}", e.what());
    }
}

void refreshTournamentScores() {
    Db::instance().tx([](Tx& t) {
        t.exec("UPDATE tournament_teams tt SET score = coalesce(("
               "  SELECT sum(e.amount) FROM xp_events e "
               "  JOIN team_members tm ON tm.student_id = e.student_id "
               "  JOIN tournaments tr ON tr.id = tt.tournament_id "
               "  WHERE tm.team_id = tt.team_id AND e.amount > 0 "
               "  AND e.created_at BETWEEN tr.starts_at AND tr.ends_at), 0), updated_at = now()");
        t.exec("WITH ranked AS (SELECT tournament_id, team_id, row_number() OVER ("
               "PARTITION BY tournament_id ORDER BY score DESC) AS r FROM tournament_teams) "
               "UPDATE tournament_teams tt SET rank = ranked.r FROM ranked "
               "WHERE tt.tournament_id = ranked.tournament_id AND tt.team_id = ranked.team_id");
    });
}

void decayMastery() {
    Db::instance().tx([](Tx& t) {
        t.exec("UPDATE topic_mastery SET mastery = round((0.3 + (mastery - 0.3) * "
               "power(0.5, extract(epoch FROM now() - last_seen_at) / (86400.0 * 21)))::numeric, 4), "
               "updated_at = now() WHERE last_seen_at IS NOT NULL "
               "AND last_seen_at < now() - interval '1 day'");
    });
}

void resetBrokenStreaks() {
    Db::instance().tx([](Tx& t) {
        // Consume a streak freeze when available, otherwise reset the streak.
        t.exec("WITH stale AS (SELECT s.user_id FROM students s WHERE s.current_streak > 0 "
               "AND (s.last_activity_date IS NULL OR s.last_activity_date < "
               " (now() AT TIME ZONE 'Asia/Qyzylorda')::date - 1)), "
               "freeze_items AS (SELECT i.student_id, i.item_id FROM inventory i "
               " JOIN shop_items si ON si.id = i.item_id AND si.kind = 'streak_freeze' "
               " WHERE i.qty > 0 AND i.student_id IN (SELECT user_id FROM stale) LIMIT 1) "
               "UPDATE inventory i SET qty = i.qty - 1 FROM freeze_items f "
               "WHERE i.student_id = f.student_id AND i.item_id = f.item_id");
        t.exec("UPDATE students SET current_streak = 0, updated_at = now() "
               "WHERE current_streak > 0 AND (last_activity_date IS NULL OR "
               "last_activity_date < (now() AT TIME ZONE 'Asia/Qyzylorda')::date - 1) "
               "AND user_id NOT IN (SELECT student_id FROM inventory WHERE qty > 0)");
    });
}

void weeklyLeagueShift() {
    const std::string week = Db::instance().tx([](Tx& t) {
        return t.scalar("SELECT date_trunc('week', now() AT TIME ZONE 'Asia/Qyzylorda')::date::text")
            .value_or("");
    });
    if (week.empty() || week == g_lastWeekly) return;
    try {
        Db::instance().tx([](Tx& t) {
            auto rows = t.query(
                "WITH weekly AS (SELECT s.user_id, s.league, coalesce(sum(e.amount), 0) AS xp "
                "FROM students s LEFT JOIN xp_events e ON e.student_id = s.user_id AND e.amount > 0 "
                "AND e.created_at >= date_trunc('week', now()) GROUP BY s.user_id, s.league), "
                "ranked AS (SELECT *, row_number() OVER (PARTITION BY league ORDER BY xp DESC) AS r, "
                "count(*) OVER (PARTITION BY league) AS n FROM weekly) "
                "SELECT user_id, league, r, n FROM ranked WHERE r <= 3 OR r > n - 3");
            for (const auto& row : rows) {
                const std::string league = row["league"];
                const int r = row.value("r", 0);
                const int n = row.value("n", 0);
                static const char* order[] = {"bronze", "silver", "gold", "platinum", "diamond"};
                int idx = 0;
                for (int i = 0; i < 5; ++i) {
                    if (league == order[i]) idx = i;
                }
                std::string next = league;
                if (r <= 3 && idx < 4) next = order[idx + 1];
                else if (r > n - 3 && idx > 0 && n > 10) next = order[idx - 1];
                if (next != league) {
                    t.exec("UPDATE students SET league = $1 WHERE user_id = $2",
                           {param(next), param(row["user_id"].get<std::string>())});
                }
            }
        });
        g_lastWeekly = week;
        Log::get()->info("weekly league shift applied for week {}", week);
    } catch (const std::exception& e) {
        Log::get()->error("league shift failed: {}", e.what());
    }
}

void cleanup() {
    Db::instance().tx([](Tx& t) {
        t.exec("DELETE FROM notifications WHERE created_at < now() - interval '90 days'");
        t.exec("DELETE FROM refresh_tokens WHERE expires_at < now() - interval '30 days'");
    });
}
} // namespace

void Worker::runOnce() {
    std::lock_guard lock(g_onceMutex);
    try {
        activateSeasons();
        closeFinishedSeasons();
        dailySnapshots();
        refreshTournamentScores();
        decayMastery();
        resetBrokenStreaks();
        weeklyLeagueShift();
        cleanup();
    } catch (const std::exception& e) {
        Log::get()->error("worker iteration failed: {}", e.what());
    }
}

void Worker::start(int intervalSeconds) {
    if (g_thread.joinable()) return;
    g_stop = false;
    g_thread = std::thread([intervalSeconds] {
        Log::get()->info("worker started (interval {}s)", intervalSeconds);
        while (!g_stop.load()) {
            Worker::runOnce();
            for (int i = 0; i < intervalSeconds && !g_stop.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    });
}

void Worker::stop() {
    g_stop = true;
    if (g_thread.joinable()) g_thread.join();
}

} // namespace lumina::jobs

