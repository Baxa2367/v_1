#include "services/common.h"
#include "core/i18n.h"
#include "core/util.h"
#include "services/xp_engine.h"
#include <regex>

namespace lumina::svc {

bool isUuid(const std::string& s) {
    static const std::regex re(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$");
    return std::regex_match(s, re);
}

json xpProgress(long long totalXp, const std::string& locale) {
    const int level = XpEngine::levelForXp(totalXp);
    long long floorXp = 0;
    for (int l = 1; l < level; ++l) floorXp += XpEngine::xpToNext(l);
    const int next = XpEngine::xpToNext(level);
    const long long inLevel = totalXp - floorXp;
    return {
        {"total_xp", totalXp},
        {"level", level},
        {"title", XpEngine::titleForLevel(level, locale)},
        {"next_title", XpEngine::titleForLevel(level + 1, locale)},
        {"xp_in_level", inLevel},
        {"xp_to_next", next},
        {"progress", next > 0 ? static_cast<double>(inLevel) / next : 0.0},
    };
}

json loadUser(Tx& t, const std::string& userId, const std::string& locale) {
    auto u = t.one("SELECT id, email, role, locale, status, display_name, created_at "
                   "FROM users WHERE id = $1",
                   {param(userId)});
    if (u.is_null()) throw notFound("User not found");
    return u;
}

json loadStudent(Tx& t, const std::string& userId, const std::string& locale) {
    auto row = t.one(
        "SELECT s.*, u.email, u.role, u.status, u.locale AS user_locale, u.display_name "
        "FROM students s JOIN users u ON u.id = s.user_id WHERE s.user_id = $1",
        {param(userId)});
    if (row.is_null()) throw notFound("Student profile not found");

    const long long totalXp = row.value("total_xp", 0LL);
    auto progress = xpProgress(totalXp, locale);

    json out = {
        {"id", row["user_id"]},
        {"email", row["email"]},
        {"role", row["role"]},
        {"status", row["status"]},
        {"locale", row["user_locale"]},
        {"display_name", row["display_name"]},
        {"nickname", row["nickname"]},
        {"avatar", row["avatar"]},
        {"bio", row["bio"]},
        {"total_xp", totalXp},
        {"level", progress["level"]},
        {"title", progress["title"]},
        {"xp_in_level", progress["xp_in_level"]},
        {"xp_to_next", progress["xp_to_next"]},
        {"level_progress", progress["progress"]},
        {"coins", row.value("coins", 0)},
        {"gems", row.value("gems", 0)},
        {"current_streak", row.value("current_streak", 0)},
        {"longest_streak", row.value("longest_streak", 0)},
        {"daily_goal_xp", row.value("daily_goal_xp", 50)},
        {"league", row["league"]},
        {"privacy_flags", row["privacy_flags"]},
        {"showcase", row["showcase"]},
        {"last_active_at", row["last_active_at"]},
    };
    return out;
}

json studentPublic(const json& row, const std::string& locale) {
    const int level = row.value("level", 1);
    return {
        {"student_id", row.value("user_id", row.value("student_id", ""))},
        {"nickname", row.value("nickname", "")},
        {"avatar", row.value("avatar", "face")},
        {"level", level},
        {"title", XpEngine::titleForLevel(level, locale)},
        {"league", row.value("league", "bronze")},
        {"total_xp", row.value("total_xp", 0LL)},
    };
}

std::string activeSeasonId(Tx& t) {
    auto s = t.scalar("SELECT id FROM seasons WHERE status = 'active' "
                      "AND now() BETWEEN starts_at AND ends_at ORDER BY starts_at DESC LIMIT 1");
    return s.value_or("");
}

void notify(Tx& t, const std::string& userId, const std::string& kind, const json& title,
            const json& body, const json& payload) {
    t.exec("INSERT INTO notifications (id, user_id, kind, title, body, payload) "
           "VALUES ($1, $2, $3, $4::jsonb, $5::jsonb, $6::jsonb)",
           {param(uuidV4()), param(userId), param(kind), paramJson(title), paramJson(body),
            paramJson(payload)});
}

long long addXp(Tx& t, const std::string& studentId, int amount, const std::string& reason,
                const std::string& sourceType, const std::string& sourceId,
                const std::string& courseId, const std::string& seasonId,
                long long balanceBefore) {
    const long long balanceAfter = balanceBefore + amount;
    t.exec("INSERT INTO xp_events (student_id, amount, reason, source_type, source_id, "
           "balance_after, course_id, season_id) "
           "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)",
           {param(studentId), param(amount), param(reason), param(sourceType), param(sourceId),
            param(balanceAfter), courseId.empty() ? std::optional<std::string>() : param(courseId),
            seasonId.empty() ? std::optional<std::string>() : param(seasonId)});
    return balanceAfter;
}

void ensureEnrollment(Tx& t, const std::string& studentId, const std::string& courseId) {
    t.exec("INSERT INTO course_enrollments (student_id, course_id) VALUES ($1, $2) "
           "ON CONFLICT (student_id, course_id) DO NOTHING",
           {param(studentId), param(courseId)});
}

std::string todayLocal(Tx& t) {
    return t.scalar("SELECT (now() AT TIME ZONE 'Asia/Qyzylorda')::date::text").value_or("");
}

std::string weekStartLocal(Tx& t) {
    return t.scalar(
               "SELECT date_trunc('week', now() AT TIME ZONE 'Asia/Qyzylorda')::date::text")
        .value_or("");
}

} // namespace lumina::svc
