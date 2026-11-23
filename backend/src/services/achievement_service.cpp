#include "services/achievement_service.h"
#include "core/i18n.h"
#include "core/util.h"
#include "services/xp_engine.h"
#include "services/achievement_service.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace lumina {

namespace {

double num(const json& j, double def = 0.0) {
    if (j.is_number()) return j.get<double>();
    if (j.is_string()) {
        try {
            return std::stod(j.get<std::string>());
        } catch (...) {
        }
    }
    return def;
}

bool compare(double value, const std::string& op, double target) {
    if (op == ">=") return value >= target;
    if (op == ">") return value > target;
    if (op == "<=") return value <= target;
    if (op == "<") return value < target;
    if (op == "==") return std::abs(value - target) < 1e-9;
    return false;
}

json localizeAchievement(const json& row, const std::string& locale) {
    json out = row;
    out["title"] = I18n::pick(row["title"], locale);
    out["description"] = I18n::pick(row["description"], locale);
    return out;
}

} // namespace

bool AchievementService::matches(const json& rule, const std::string& event) {
    const std::string ev = rule.value("event", "any");
    return ev == "any" || ev == event;
}

double AchievementService::metricValue(const json& metrics, const std::string& name) {
    if (metrics.contains(name)) return num(metrics[name]);
    return 0.0;
}

json AchievementService::snapshot(Tx& t, const std::string& studentId) {
    json m = json::object();

    auto s = t.one("SELECT total_xp, level, current_streak, longest_streak, coins "
                   "FROM students WHERE user_id = $1",
                   {param(studentId)});
    m["xp_total"] = s.is_null() ? 0 : s.value("total_xp", 0LL);
    m["level"] = s.is_null() ? 1 : s.value("level", 1);
    m["current_streak"] = s.is_null() ? 0 : s.value("current_streak", 0);
    m["longest_streak"] = s.is_null() ? 0 : s.value("longest_streak", 0);

    auto a = t.one(
        "SELECT count(*) FILTER (WHERE result = 'correct') AS correct, "
        "count(*) AS attempts, coalesce(avg(score) * 100, 0) AS accuracy, "
        "count(DISTINCT t.topic_id) AS distinct_topics, "
        "count(*) FILTER (WHERE result = 'correct' AND hints_used = 0) AS no_hint_correct, "
        "count(*) FILTER (WHERE result = 'correct' AND time_spent_ms < 5000) AS fast_correct, "
        "count(*) FILTER (WHERE result = 'correct' AND extract(hour from "
        " (at.created_at AT TIME ZONE 'Asia/Qyzylorda')) BETWEEN 0 AND 4) AS night_correct, "
        "count(*) FILTER (WHERE result = 'correct' AND extract(hour from "
        " (at.created_at AT TIME ZONE 'Asia/Qyzylorda')) BETWEEN 5 AND 7) AS early_correct, "
        "count(*) FILTER (WHERE result = 'correct' AND t.difficulty >= 8) AS hard_correct "
        "FROM attempts at JOIN tasks t ON t.id = at.task_id WHERE at.student_id = $1",
        {param(studentId)});
    m["tasks_completed"] = a.value("correct", 0LL);
    m["attempts"] = a.value("attempts", 0LL);
    m["accuracy_pct"] = num(a["accuracy"]);
    m["distinct_topics"] = a.value("distinct_topics", 0LL);
    m["no_hint_correct"] = a.value("no_hint_correct", 0LL);
    m["fast_correct"] = a.value("fast_correct", 0LL);
    m["night_correct"] = a.value("night_correct", 0LL);
    m["early_correct"] = a.value("early_correct", 0LL);
    m["hard_correct"] = a.value("hard_correct", 0LL);

    auto lv = t.one(
        "SELECT count(*) FILTER (WHERE status = 'completed') AS completed, "
        "count(*) FILTER (WHERE stars = 3) AS perfect, "
        "count(*) FILTER (WHERE status = 'completed' AND l.is_boss) AS boss "
        "FROM level_progress lp JOIN levels l ON l.id = lp.level_id "
        "WHERE lp.student_id = $1",
        {param(studentId)});
    m["levels_completed"] = lv.value("completed", 0LL);
    m["perfect_levels"] = lv.value("perfect", 0LL);
    m["boss_completed"] = lv.value("boss", 0LL);

    auto streakMax = t.scalar(
        "WITH s AS (SELECT result, row_number() OVER (ORDER BY created_at) rn "
        " FROM attempts WHERE student_id = $1), "
        "g AS (SELECT result, rn - row_number() OVER (PARTITION BY result ORDER BY rn) grp FROM s) "
        "SELECT coalesce(max(c), 0) FROM (SELECT count(*) c FROM g WHERE result = 'correct' "
        "GROUP BY grp) x",
        {param(studentId)});
    m["correct_streak_max"] = streakMax ? std::stoll(*streakMax) : 0;

    auto noHintStreak = t.scalar(
        "WITH s AS (SELECT (result = 'correct' AND hints_used = 0) ok, "
        " row_number() OVER (ORDER BY created_at) rn FROM attempts WHERE student_id = $1), "
        "g AS (SELECT ok, rn - row_number() OVER (PARTITION BY ok ORDER BY rn) grp FROM s) "
        "SELECT coalesce(max(c), 0) FROM (SELECT count(*) c FROM g WHERE ok GROUP BY grp) x",
        {param(studentId)});
    m["no_hint_streak_max"] = noHintStreak ? std::stoll(*noHintStreak) : 0;

    m["topics_mastered"] = t.scalarInt(
        "SELECT count(*) FROM topic_mastery WHERE student_id = $1 AND mastery >= 0.8",
        {param(studentId)});
    m["courses_completed"] = t.scalarInt(
        "SELECT count(*) FROM course_enrollments WHERE student_id = $1 AND status = 'completed'",
        {param(studentId)});
    m["coins_spent"] = t.scalarInt(
        "SELECT coalesce(sum(price_coins), 0) FROM purchases WHERE student_id = $1",
        {param(studentId)});
    m["quests_completed"] = t.scalarInt(
        "SELECT count(*) FROM student_quests WHERE student_id = $1 "
        "AND status IN ('completed', 'claimed')",
        {param(studentId)});
    m["team_member"] = t.scalarInt("SELECT count(*) FROM team_members WHERE student_id = $1",
                                   {param(studentId)}) > 0 ? 1 : 0;
    m["team_wins"] = t.scalarInt(
        "SELECT count(*) FROM tournament_teams tt JOIN team_members tm ON tm.team_id = tt.team_id "
        "JOIN tournaments tr ON tr.id = tt.tournament_id "
        "WHERE tm.student_id = $1 AND tt.rank = 1 AND tr.status = 'closed'",
        {param(studentId)});

    m["fixed_mistakes"] = t.scalarInt(
        "SELECT count(*) FROM (SELECT task_id FROM attempts WHERE student_id = $1 "
        "GROUP BY task_id HAVING sum(CASE WHEN result = 'wrong' THEN 1 ELSE 0 END) >= 3 "
        "AND max(created_at) FILTER (WHERE result = 'correct') IS NOT NULL) x",
        {param(studentId)});

    m["weekly_xp"] = t.scalarInt(
        "SELECT coalesce(sum(amount), 0) FROM xp_events WHERE student_id = $1 AND amount > 0 "
        "AND created_at >= date_trunc('week', now())",
        {param(studentId)});

    auto season = t.one("SELECT id FROM seasons WHERE status = 'active' "
                        "AND now() BETWEEN starts_at AND ends_at ORDER BY starts_at DESC LIMIT 1");
    if (!season.is_null()) {
        const std::string sid = season["id"].get<std::string>();
        const long long myXp = t.scalarInt(
            "SELECT season_xp FROM season_progress WHERE season_id = $1 AND student_id = $2",
            {param(sid), param(studentId)});
        const long long ahead = t.scalarInt(
            "SELECT count(*) FROM season_progress WHERE season_id = $1 AND season_xp > $2",
            {param(sid), param(myXp)});
        m["season_rank_best"] = static_cast<int>(ahead + 1);
    } else {
        m["season_rank_best"] = 9999;
    }
    return m;
}

json AchievementService::evaluate(Tx& t, const std::string& studentId, const std::string& locale,
                                  const std::string& event, const json& extra) {
    auto all = t.query("SELECT * FROM achievements ORDER BY sort_order");
    auto earned = t.query("SELECT achievement_id FROM student_achievements WHERE student_id = $1",
                          {param(studentId)});
    std::set<std::string> owned;
    for (const auto& e : earned) owned.insert(e["achievement_id"].get<std::string>());

    json need;
    for (const auto& a : all) {
        if (owned.count(a["id"].get<std::string>())) continue;
        if (!matches(a["rule"], event)) continue;
        need.push_back(a);
    }
    if (need.empty()) return json::array();

    const json metrics = snapshot(t, studentId);
    json awarded = json::array();
    for (const auto& a : need) {
        const json rule = a["rule"];
        const std::string metric = rule.value("metric", "");
        if (metric.empty()) continue;
        const double value = metricValue(metrics, metric);
        const double target = num(rule.value("value", json(0)));
        const std::string op = rule.value("op", ">=");
        if (!compare(value, op, target)) continue;

        const int64_t inserted = t.exec(
            "INSERT INTO student_achievements (student_id, achievement_id, progress) "
            "VALUES ($1, $2, $3) ON CONFLICT (student_id, achievement_id) DO NOTHING",
            {param(studentId), param(a["id"].get<std::string>()), param(value)});
        if (inserted <= 0) continue;

        json item = localizeAchievement(a, locale);
        item["awarded_at"] = isoNow();
        item["progress_value"] = value;
        awarded.push_back(item);

        const int coins = a.value("reward_coins", 0);
        if (coins > 0) {
            t.exec("UPDATE students SET coins = coins + $1, updated_at = now() WHERE user_id = $2",
                   {param(coins), param(studentId)});
        }
        svc::notify(t, studentId, "achievement",
                    json{{"kk", "Жаңа жетістік!"}, {"ru", "Новое достижение!"},
                         {"en", "Achievement unlocked!"}},
                    json{{"kk", a["title"].value("kk", "")},
                         {"ru", a["title"].value("ru", "")},
                         {"en", a["title"].value("en", a["title"].value("ru", ""))}},
                    {{"achievement_id", a["id"]}, {"code", a["code"]},
                     {"rarity", a["rarity"]}, {"icon", a["icon"]}});
    }
    return awarded;
}

json AchievementService::collection(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        const json metrics = snapshot(t, ctx.userId);
        auto all = t.query("SELECT * FROM achievements ORDER BY category, sort_order");
        auto owned = t.query(
            "SELECT achievement_id, awarded_at, progress FROM student_achievements "
            "WHERE student_id = $1",
            {param(ctx.userId)});
        std::map<std::string, json> earned;
        for (const auto& e : owned) earned[e["achievement_id"].get<std::string>()] = e;

        json out = json::array();
        int unlocked = 0;
        for (const auto& a : all) {
            json item = localizeAchievement(a, ctx.locale);
            const std::string id = a["id"].get<std::string>();
            const bool has = earned.count(id) > 0;
            if (has) ++unlocked;
            item["awarded"] = has;
            item["awarded_at"] = has ? earned[id]["awarded_at"] : json(nullptr);
            const json rule = a["rule"];
            const double target = num(rule.value("value", json(1)));
            const double value = metricValue(metrics, rule.value("metric", ""));
            item["progress"] = target > 0 ? std::clamp(value / target, 0.0, 1.0) : 0.0;
            item["progress_value"] = value;
            item["target_value"] = target;
            if (has) item["progress"] = 1.0;
            out.push_back(item);
        }
        return json{{"achievements", out},
                    {"unlocked", unlocked},
                    {"total", static_cast<int>(all.size())},
                    {"metrics", metrics}};
    });
}

} // namespace lumina


