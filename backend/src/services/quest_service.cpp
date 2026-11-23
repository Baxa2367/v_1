#include "services/quest_service.h"
#include "core/i18n.h"
#include "core/util.h"
#include "services/common.h"
#include "services/achievement_service.h"

namespace lumina {

namespace {

json localizeQuest(const json& row, const std::string& locale) {
    json out = row;
    out["title"] = I18n::pick(row["title"], locale);
    out["description"] = I18n::pick(row["description"], locale);
    return out;
}

std::string kindFor(bool weekly) { return weekly ? "weekly" : "daily"; }

// Deterministically selects `count` quests from `rows` for a given seed.
std::vector<json> selectQuests(const json& rows, const std::string& seed, int count) {
    std::vector<json> out;
    if (rows.empty()) return out;
    size_t h = std::hash<std::string>{}(seed);
    std::vector<size_t> idx(rows.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    for (size_t i = idx.size(); i > 1; --i) {
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdULL;
        std::swap(idx[i - 1], idx[h % i]);
    }
    const int take = std::min<int>(count, static_cast<int>(idx.size()));
    for (int i = 0; i < take; ++i) out.push_back(rows[idx[i]]);
    return out;
}

void ensureAssigned(Tx& t, const std::string& studentId, bool weekly) {
    const std::string dateExpr = weekly
        ? "date_trunc('week', now() AT TIME ZONE 'Asia/Qyzylorda')::date"
        : "(now() AT TIME ZONE 'Asia/Qyzylorda')::date";
    const std::string date = t.scalar("SELECT " + dateExpr + "::text").value_or("");
    if (date.empty()) return;

    const auto existing = t.one(
        "SELECT count(*) AS c FROM student_quests sq JOIN daily_quests dq ON dq.id = sq.quest_id "
        "WHERE sq.student_id = $1 AND sq.quest_date = $2::date AND dq.kind = $3",
        {param(studentId), param(date), param(kindFor(weekly))});
    if (existing.value("c", 0) > 0) return;

    auto pool = t.query("SELECT * FROM daily_quests WHERE active AND kind = $1 ORDER BY code",
                        {param(kindFor(weekly))});
    auto picked = selectQuests(pool, studentId + (weekly ? ":week" : ":day") + date, weekly ? 2 : 3);
    for (const auto& q : picked) {
        t.exec("INSERT INTO student_quests (id, student_id, quest_id, quest_date, target) "
               "VALUES ($1, $2, $3, $4::date, $5) ON CONFLICT DO NOTHING",
               {param(uuidV4()), param(studentId), param(q["id"].get<std::string>()), param(date),
                param(q.value("target", 1))});
    }
}

double ruleValue(Tx& t, const json& rule, const std::string& studentId,
                 const std::string& from, const std::string& to) {
    const std::string type = rule.value("type", "");
    if (type == "solve_tasks" || type == "correct_tasks") {
        return static_cast<double>(t.scalarInt(
            "SELECT count(DISTINCT a.task_id) FROM attempts a WHERE a.student_id = $1 "
            "AND a.result = 'correct' AND a.created_at >= $2::date AND a.created_at < $3::date",
            {param(studentId), param(from), param(to)}));
    }
    if (type == "accuracy") {
        return t.scalarDouble(
            "SELECT coalesce(avg(score) * 100, 0) FROM attempts WHERE student_id = $1 "
            "AND created_at >= $2::date AND created_at < $3::date",
            {param(studentId), param(from), param(to)});
    }
    if (type == "no_hints") {
        return static_cast<double>(t.scalarInt(
            "SELECT count(*) FROM attempts WHERE student_id = $1 AND hints_used = 0 "
            "AND result = 'correct' AND created_at >= $2::date AND created_at < $3::date",
            {param(studentId), param(from), param(to)}));
    }
    if (type == "xp_gain") {
        return static_cast<double>(t.scalarInt(
            "SELECT coalesce(sum(amount), 0) FROM xp_events WHERE student_id = $1 AND amount > 0 "
            "AND created_at >= $2::date AND created_at < $3::date",
            {param(studentId), param(from), param(to)}));
    }
    if (type == "correct_streak") {
        return static_cast<double>(t.scalarInt(
            "WITH s AS (SELECT result, row_number() OVER (ORDER BY created_at) rn FROM attempts "
            " WHERE student_id = $1 AND created_at >= $2::date AND created_at < $3::date), "
            "g AS (SELECT result, rn - row_number() OVER (PARTITION BY result ORDER BY rn) grp FROM s) "
            "SELECT coalesce(max(c), 0) FROM (SELECT count(*) c FROM g WHERE result = 'correct' "
            "GROUP BY grp) x",
            {param(studentId), param(from), param(to)}));
    }
    if (type == "complete_level") {
        return static_cast<double>(t.scalarInt(
            "SELECT count(*) FROM level_progress WHERE student_id = $1 AND status = 'completed' "
            "AND completed_at >= $2::date AND completed_at < $3::date",
            {param(studentId), param(from), param(to)}));
    }
    if (type == "distinct_topics") {
        return static_cast<double>(t.scalarInt(
            "SELECT count(DISTINCT tk.topic_id) FROM attempts a JOIN tasks tk ON tk.id = a.task_id "
            "WHERE a.student_id = $1 AND a.created_at >= $2::date AND a.created_at < $3::date",
            {param(studentId), param(from), param(to)}));
    }
    if (type == "streak_days") {
        return static_cast<double>(t.scalarInt(
            "SELECT current_streak FROM students WHERE user_id = $1", {param(studentId)}));
    }
    if (type == "hard_tasks") {
        return static_cast<double>(t.scalarInt(
            "SELECT count(DISTINCT a.task_id) FROM attempts a JOIN tasks tk ON tk.id = a.task_id "
            "WHERE a.student_id = $1 AND a.result = 'correct' AND tk.difficulty >= 7 "
            "AND a.created_at >= $2::date AND a.created_at < $3::date",
            {param(studentId), param(from), param(to)}));
    }
    return 0.0;
}

void refreshRange(Tx& t, const std::string& studentId, bool weekly) {
    const std::string from = weekly
        ? t.scalar("SELECT date_trunc('week', now() AT TIME ZONE 'Asia/Qyzylorda')::date::text").value_or("")
        : t.scalar("SELECT (now() AT TIME ZONE 'Asia/Qyzylorda')::date::text").value_or("");
    if (from.empty()) return;
    const std::string to = t.scalar("SELECT ($1::date + CASE WHEN $2 THEN 7 ELSE 1 END)::text",
                                    {param(from), param(weekly)}).value_or("");
    if (to.empty()) return;

    auto rows = t.query(
        "SELECT sq.id, sq.progress, sq.target, sq.status, dq.rule FROM student_quests sq "
        "JOIN daily_quests dq ON dq.id = sq.quest_id "
        "WHERE sq.student_id = $1 AND sq.quest_date = $2::date",
        {param(studentId), param(from)});
    for (const auto& row : rows) {
        if (row.value("status", "") == "claimed") continue;
        const double value = ruleValue(t, row["rule"], studentId, from, to);
        const int target = row.value("target", 1);
        const int progress = std::min(static_cast<int>(std::lround(value)), target);
        const bool done = progress >= target;
        t.exec("UPDATE student_quests SET progress = $2, "
               "status = CASE WHEN $3 AND status = 'active' THEN 'completed' ELSE status END, "
               "completed_at = CASE WHEN $3 AND completed_at IS NULL THEN now() ELSE completed_at END "
               "WHERE id = $1",
               {param(row["id"].get<std::string>()), param(progress), param(done)});
    }
}

} // namespace

void QuestService::refresh(Tx& t, const std::string& studentId) {
    ensureAssigned(t, studentId, false);
    ensureAssigned(t, studentId, true);
    refreshRange(t, studentId, false);
    refreshRange(t, studentId, true);
}

json QuestService::list(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        refresh(t, ctx.userId);
        auto rows = t.query(
            "SELECT sq.id, sq.progress, sq.target, sq.status, sq.completed_at, sq.claimed_at, "
            "sq.quest_date, dq.code, dq.title, dq.description, dq.kind, dq.reward_xp, "
            "dq.reward_coins, dq.icon "
            "FROM student_quests sq JOIN daily_quests dq ON dq.id = sq.quest_id "
            "WHERE sq.student_id = $1 AND sq.quest_date >= date_trunc('week', now() "
            " AT TIME ZONE 'Asia/Qyzylorda')::date ORDER BY sq.quest_date DESC, dq.kind, dq.code",
            {param(ctx.userId)});
        json out = json::array();
        int dailyDone = 0, dailyTotal = 0;
        for (const auto& row : rows) {
            json q = localizeQuest(row, ctx.locale);
            q["progress"] = std::min(row.value("progress", 0), row.value("target", 1));
            if (row.value("kind", "daily") == "daily") {
                ++dailyTotal;
                if (row.value("status", "") != "active") ++dailyDone;
            }
            out.push_back(q);
        }
        return json{{"quests", out},
                    {"daily_completed", dailyDone},
                    {"daily_total", dailyTotal},
                    {"bonus_available", dailyDone >= 3}};
    });
}

json QuestService::claim(const AuthCtx& ctx, const std::string& studentQuestId) {
    return Db::instance().tx([&](Tx& t) {
        auto q = t.one(
            "SELECT sq.id, sq.student_id, sq.status, sq.quest_date, dq.title, dq.reward_xp, "
            "dq.reward_coins, dq.kind FROM student_quests sq JOIN daily_quests dq ON dq.id = sq.quest_id "
            "WHERE sq.id = $1 AND sq.student_id = $2",
            {param(studentQuestId), param(ctx.userId)});
        if (q.is_null()) throw notFound("Quest not found");
        if (q.value("status", "") == "claimed") throw conflict("Quest already claimed");
        if (q.value("status", "") != "completed") throw badRequest("Quest is not completed yet");

        t.exec("UPDATE student_quests SET status = 'claimed', claimed_at = now() WHERE id = $1",
               {param(studentQuestId)});

        auto student = t.one("SELECT total_xp, coins FROM students WHERE user_id = $1 FOR UPDATE",
                             {param(ctx.userId)});
        long long xp = student.value("total_xp", 0LL);
        const int rewardXp = q.value("reward_xp", 0);
        const int rewardCoins = q.value("reward_coins", 0);
        const std::string season = svc::activeSeasonId(t);
        if (rewardXp > 0) {
            xp = svc::addXp(t, ctx.userId, rewardXp, "quest_reward", "quest",
                            studentQuestId, "", season, xp);
        }
        t.exec("UPDATE students SET total_xp = $1, coins = coins + $2, updated_at = now() "
               "WHERE user_id = $3",
               {param(xp), param(rewardCoins), param(ctx.userId)});

        auto achievements =
            AchievementService::evaluate(t, ctx.userId, ctx.locale, "quest_completed");
        return json{{"claimed", true},
                    {"reward_xp", rewardXp},
                    {"reward_coins", rewardCoins},
                    {"achievements", achievements}};
    });
}

json QuestService::onLevelCompleted(Tx& t, const std::string& studentId, const std::string& levelId) {
    (void)levelId;
    refresh(t, studentId);
    return json::object();
}

} // namespace lumina

