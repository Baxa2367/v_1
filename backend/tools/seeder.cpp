#include "ai/mastery_model.h"
#include "core/config.h"
#include "core/crypto.h"
#include "core/i18n.h"
#include "core/log.h"
#include "core/util.h"
#include "db/db.h"
#include "services/achievement_service.h"
#include "services/leaderboard_service.h"
#include "services/xp_engine.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace lumina;
namespace fs = std::filesystem;

namespace {

json readJson(const fs::path& p) {
    std::ifstream in(p);
    if (!in) throw std::runtime_error("cannot open " + p.string());
    json j;
    in >> j;
    return j;
}

const char* kPassword = "Lumina2026!";

struct TaskInfo {
    std::string id, levelId, topicId;
    int difficulty = 1;
    int baseXp = 30;
    int timeLimit = 120;
    int orderNo = 1;
    bool boss = false;
};

struct SimAttempt {
    TaskInfo task;
    int minutesAgo = 0;
    double score = 0;
    std::string result;
    int timeMs = 0;
    int hints = 0;
    int xp = 0;
};

struct DemoStudent {
    std::string userId;
    std::string nickname;
    std::string profile;
    std::string courseId;
    std::string courseTitle;
    std::vector<SimAttempt> attempts;
    std::map<std::string, ai::MasteryState> mastery;
    std::map<std::string, double> taskBest;
    std::map<std::string, std::vector<double>> levelScores;
    std::map<std::string, int> levelOrder;
    std::map<std::string, int> levelXpReward;
    std::map<std::string, bool> levelBoss;
    std::map<std::string, int> dailyXp;
    long long totalXp = 0;
    int level = 1;
    int coins = 0;
    int currentStreak = 0;
    int longestStreak = 0;
    int dailyGoal = 50;
    std::string league = "bronze";
};

std::string createUser(Tx& t, const std::string& email, const std::string& name,
                       const std::string& role, const std::string& locale,
                       const std::string& passwordHash, const std::string& nickname,
                       const std::string& avatar) {
    const std::string id = uuidV4();
    t.exec("INSERT INTO users (id, email, password_hash, role, locale, display_name) "
           "VALUES ($1,$2,$3,$4,$5,$6)",
           {param(id), param(email), param(passwordHash), param(role), param(locale), param(name)});
    if (role == "student") {
        t.exec("INSERT INTO students (user_id, nickname, avatar) VALUES ($1,$2,$3)",
               {param(id), param(nickname), param(avatar)});
    }
    return id;
}

void seedCourse(Tx& t, const json& course, const std::string& teacherId) {
    const std::string slug = course.value("slug", "");
    if (slug.empty()) throw std::runtime_error("course without slug");
    if (t.scalar("SELECT 1 FROM courses WHERE slug = $1", {param(slug)})) {
        Log::get()->info("course {} already present, skipping", slug);
        return;
    }
    const std::string courseId = uuidV4();
    t.exec("INSERT INTO courses (id, slug, title, description, category, tags, cover, difficulty, "
           "status, author_id, sort_order) VALUES ($1,$2,$3::jsonb,$4::jsonb,$5,$6::jsonb,$7,$8,"
           "'published',$9,$10)",
           {param(courseId), param(slug), paramJson(course["title"]),
            paramJson(course["description"]), param(course.value("category", "general")),
            paramJson(course.value("tags", json::array())), param(course.value("cover", "school")),
            param(course.value("difficulty", 3)), param(teacherId),
            param(course.value("sort_order", 0))});

    std::map<std::string, std::string> topicIds;
    const auto& levels = course.at("levels");
    for (const auto& level : levels) {
        for (const auto& topic : level.value("topics", json::array())) {
            const std::string code = topic.value("code", "");
            if (topicIds.count(code)) continue;
            const std::string topicId = uuidV4();
            t.exec("INSERT INTO topics (id, course_id, code, name) VALUES ($1,$2,$3,$4::jsonb)",
                   {param(topicId), param(courseId), param(code), paramJson(topic["name"])});
            topicIds[code] = topicId;
        }
    }

    for (const auto& level : levels) {
        const std::string levelId = uuidV4();
        t.exec("INSERT INTO levels (id, course_id, order_no, title, description, icon, unlock_rules, "
               "is_boss, xp_reward) VALUES ($1,$2,$3,$4::jsonb,$5::jsonb,$6,$7::jsonb,$8,$9)",
               {param(levelId), param(courseId), param(level.value("order_no", 1)),
                paramJson(level["title"]), paramJson(level["description"]),
                param(level.value("icon", "flag")),
                paramJson(level.value("unlock_rules", json::object())),
                param(level.value("is_boss", false)), param(level.value("xp_reward", 120))});

        int sort = 1;
        for (const auto& task : level.at("tasks")) {
            const std::string topicCode = task.value("topic", "");
            if (!topicIds.count(topicCode)) {
                throw std::runtime_error("unknown topic " + topicCode + " in " + slug);
            }
            t.exec("INSERT INTO tasks (id, level_id, topic_id, type, prompt, payload, answer_key, "
                   "explanation, hints, difficulty, base_xp, time_limit_sec, generated, status, "
                   "sort_order) VALUES ($1,$2,$3,$4,$5::jsonb,$6::jsonb,$7::jsonb,$8::jsonb,"
                   "$9::jsonb,$10,$11,$12,false,'published',$13)",
                   {param(uuidV4()), param(levelId), param(topicIds[topicCode]),
                    param(task.value("type", "single_choice")), paramJson(task["prompt"]),
                    paramJson(task.value("payload", json::object())),
                    paramJson(task["answer_key"]),
                    paramJson(task.value("explanation", json::object())),
                    paramJson(task.value("hints", json::array())),
                    param(task.value("difficulty", 3)), param(task.value("base_xp", 30)),
                    param(task.value("time_limit_sec", 120)), param(sort++)});
        }
    }
    Log::get()->info("seeded course {} ({} levels)", slug, levels.size());
}

void seedAchievements(Tx& t, const json& items) {
    int synced = 0;
    for (const auto& a : items) {
        t.exec("INSERT INTO achievements (id, code, category, rarity, rule, title, description, icon, "
               "points, reward_coins, is_secret, sort_order) "
               "VALUES ($1,$2,$3,$4,$5::jsonb,$6::jsonb,$7::jsonb,$8,$9,$10,$11,$12) "
               "ON CONFLICT (code) DO UPDATE SET category = EXCLUDED.category, rarity = EXCLUDED.rarity, "
               "rule = EXCLUDED.rule, title = EXCLUDED.title, description = EXCLUDED.description, "
               "icon = EXCLUDED.icon, points = EXCLUDED.points, reward_coins = EXCLUDED.reward_coins, "
               "is_secret = EXCLUDED.is_secret, sort_order = EXCLUDED.sort_order",
               {param(uuidV4()), param(a["code"]), param(a.value("category", "start")),
                param(a.value("rarity", "common")), paramJson(a["rule"]), paramJson(a["title"]),
                paramJson(a["description"]), param(a.value("icon", "emoji_events")),
                param(a.value("points", 10)), param(a.value("reward_coins", 0)),
                param(a.value("is_secret", false)), param(a.value("sort_order", 0))});
        ++synced;
    }
    Log::get()->info("achievements synced: {}", synced);
}

void seedShop(Tx& t, const json& items) {
    int synced = 0;
    for (const auto& i : items) {
        t.exec("INSERT INTO shop_items (id, code, kind, title, description, price_coins, price_gems, "
               "icon, payload, active, sort_order) VALUES ($1,$2,$3,$4::jsonb,$5::jsonb,$6,$7,$8,"
               "$9::jsonb,true,$10) "
               "ON CONFLICT (code) DO UPDATE SET kind = EXCLUDED.kind, title = EXCLUDED.title, "
               "description = EXCLUDED.description, price_coins = EXCLUDED.price_coins, "
               "price_gems = EXCLUDED.price_gems, icon = EXCLUDED.icon, payload = EXCLUDED.payload, "
               "active = EXCLUDED.active, sort_order = EXCLUDED.sort_order",
               {param(uuidV4()), param(i["code"]), param(i.value("kind", "hint_pack")),
                paramJson(i["title"]), paramJson(i.value("description", json::object())),
                param(i.value("price_coins", 100)), param(i.value("price_gems", 0)),
                param(i.value("icon", "shopping_bag")),
                paramJson(i.value("payload", json::object())), param(i.value("sort_order", 0))});
        ++synced;
    }
    Log::get()->info("shop items synced: {}", synced);
}

void seedQuests(Tx& t, const json& items) {
    int added = 0;
    for (const auto& q : items) {
        if (t.scalar("SELECT 1 FROM daily_quests WHERE code = $1", {param(q["code"])})) continue;
        t.exec("INSERT INTO daily_quests (id, code, title, description, kind, rule, target, reward_xp, "
               "reward_coins, icon, active) VALUES ($1,$2,$3::jsonb,$4::jsonb,$5,$6::jsonb,$7,$8,$9,"
               "$10,true)",
               {param(uuidV4()), param(q["code"]), paramJson(q["title"]),
                paramJson(q.value("description", json::object())), param(q.value("kind", "daily")),
                paramJson(q["rule"]), param(q.value("target", 1)), param(q.value("reward_xp", 20)),
                param(q.value("reward_coins", 5)), param(q.value("icon", "task_alt"))});
        ++added;
    }
    Log::get()->info("quests seeded: {}", added);
}

void seedTips(Tx& t, const json& items) {
    int added = 0;
    for (const auto& tip : items) {
        if (t.scalar("SELECT 1 FROM ai_tip_templates WHERE code = $1", {param(tip["code"])})) continue;
        t.exec("INSERT INTO ai_tip_templates (id, code, kind, text, tags, priority) "
               "VALUES ($1,$2,$3,$4::jsonb,$5::jsonb,$6)",
               {param(uuidV4()), param(tip["code"]), param(tip.value("kind", "methodical")),
                paramJson(tip["text"]), paramJson(tip.value("tags", json::array())),
                param(tip.value("priority", 0))});
        ++added;
    }
    Log::get()->info("ai tips seeded: {}", added);
}

std::string loadUserByEmail(Tx& t, const std::string& email) {
    return t.scalar("SELECT id::text FROM users WHERE lower(email) = $1", {param(toLower(email))})
        .value_or("");
}

void seedSeasons(Tx& t, const std::string& teacherId) {
    if (t.scalarInt("SELECT count(*) FROM seasons") > 0) {
        Log::get()->info("seasons already present, skipping");
        return;
    }
    struct SeasonSpec {
        std::string code, theme, titleKk, titleRu, titleEn, status;
        int startDays, endDays;
    };
    std::vector<SeasonSpec> seasons = {
        {"season-2026-autumn", "autumn", "Р С™РўР‡Р В·Р С–РЎвЂ“ Р СР В°РЎР‚Р В°РЎвЂћР С•Р Р…", "Р С›РЎРѓР ВµР Р…Р Р…Р С‘Р в„– Р СР В°РЎР‚Р В°РЎвЂћР С•Р Р…", "Autumn Marathon", "active", -10, 50},
        {"season-2026-summer", "summer", "Р вЂ“Р В°Р В·РўвЂњРЎвЂ№ РЎРѓР ВµРЎР‚Р С—РЎвЂ“Р Р…", "Р вЂєР ВµРЎвЂљР Р…Р С‘Р в„– РЎР‚РЎвЂ№Р Р†Р С•Р С”", "Summer Sprint", "closed", -120, -40},
        {"season-2026-spring", "spring", "Р С™РЈВ©Р С”РЎвЂљР ВµР СР С–РЎвЂ“ Р С•РЎРЏР Р…РЎС“", "Р вЂ™Р ВµРЎРѓР ВµР Р…Р Р…Р ВµР Вµ Р С—РЎР‚Р С•Р В±РЎС“Р В¶Р Т‘Р ВµР Р…Р С‘Р Вµ", "Spring Awakening", "closed", -220, -140},
    };
    int idx = 0;
    for (const auto& s : seasons) {
        const std::string id = uuidV4();
        const int start = s.startDays;
        const int end = s.endDays;
        t.exec("INSERT INTO seasons (id, code, title, theme, description, starts_at, ends_at, status, rewards) "
               "VALUES ($1,$2,$3::jsonb,$4,$5::jsonb, now() + ($6 || ' days')::interval, "
               "now() + ($7 || ' days')::interval, $8, '[]'::jsonb)",
               {param(id), param(s.code),
                paramJson({{"kk", s.titleKk}, {"ru", s.titleRu}, {"en", s.titleEn}}),
                param(s.theme),
                paramJson({{"kk", "Р СљР В°РЎС“РЎРѓРЎвЂ№Р СР Т‘РЎвЂ№РўвЂє Р В¶Р В°РЎР‚РЎвЂ№РЎРѓ: XP Р В¶Р С‘Р Р…Р В°Р С—, Р В¶Р С•РўвЂњР В°РЎР‚РЎвЂ№ РЎвЂљР С‘РЎР‚Р С–Р Вµ Р В¶Р ВµРЎвЂљРЎвЂ“РўР€РЎвЂ“Р В·."},
                           {"ru", "Р РЋР ВµР В·Р С•Р Р…Р Р…Р В°РЎРЏ Р С–Р С•Р Р…Р С”Р В°: Р Р…Р В°Р В±Р С‘РЎР‚Р В°Р в„–РЎвЂљР Вµ XP Р С‘ Р С•РЎвЂљР С”РЎР‚РЎвЂ№Р Р†Р В°Р в„–РЎвЂљР Вµ РЎвЂљР С‘РЎР‚РЎвЂ№."},
                           {"en", "Seasonal race: earn XP and unlock tiers."}}),
                param(start), param(end), param(s.status)});
        const int tiers[5] = {100, 300, 700, 1500, 3000};
        const int coins[5] = {50, 120, 250, 500, 1000};
        for (int i = 0; i < 5; ++i) {
            t.exec("INSERT INTO season_rewards (id, season_id, tier, title, description, required_xp, payload) "
                   "VALUES ($1,$2,$3,$4::jsonb,$5::jsonb,$6,$7::jsonb)",
                   {param(uuidV4()), param(id), param(i + 1),
                    paramJson({{"kk", "Р СћР С‘РЎР‚ " + std::to_string(i + 1)},
                               {"ru", "Р СћР С‘РЎР‚ " + std::to_string(i + 1)},
                               {"en", "Tier " + std::to_string(i + 1)}}),
                    paramJson({{"kk", "Р СљР В°РЎС“РЎРѓРЎвЂ№Р СР Т‘РЎвЂ№РўвЂє РЎРѓРЎвЂ№Р в„–Р В»РЎвЂ№РўвЂє"}, {"ru", "Р РЋР ВµР В·Р С•Р Р…Р Р…Р В°РЎРЏ Р Р…Р В°Р С–РЎР‚Р В°Р Т‘Р В°"},
                               {"en", "Season reward"}}),
                    param(tiers[i]),
                    paramJson({{"coins", coins[i]}, {"xp", 50 * (i + 1)}})});
        }
        ++idx;
    }
    (void)teacherId;
    Log::get()->info("seasons seeded: {}", idx);
}

std::vector<TaskInfo> loadTasks(Tx& t, const std::string& courseId) {
    std::vector<TaskInfo> out;
    for (const auto& row : t.query(
             "SELECT tk.id, tk.level_id, tk.topic_id, tk.difficulty, tk.base_xp, tk.time_limit_sec, "
             "l.order_no, l.is_boss FROM tasks tk JOIN levels l ON l.id = tk.level_id "
             "WHERE l.course_id = $1 AND tk.status = 'published' "
             "ORDER BY l.order_no, tk.sort_order",
             {param(courseId)})) {
        out.push_back({row["id"].get<std::string>(), row["level_id"].get<std::string>(),
                       row["topic_id"].get<std::string>(), row.value("difficulty", 1),
                       row.value("base_xp", 30), row.value("time_limit_sec", 120),
                       row.value("order_no", 1), row.value("is_boss", false)});
    }
    return out;
}

void simulateAndInsert(Tx& t, DemoStudent& s, const std::string& seasonId,
                       const std::string& today) {
    if (s.attempts.empty()) return;
    std::sort(s.attempts.begin(), s.attempts.end(),
              [](const SimAttempt& a, const SimAttempt& b) { return a.minutesAgo > b.minutesAgo; });

    long long balance = 0;
    int correctStreak = 0;
    std::set<std::string> days;
    for (const auto& a : s.attempts) {
        const std::string attemptId = uuidV4();
        t.exec("INSERT INTO attempts (id, student_id, task_id, attempt_token, answer, result, score, "
               "time_spent_ms, hints_used, context, suspicious, ip, created_at) "
               "VALUES ($1,$2,$3,$4,$5::jsonb,$6,$7,$8,$9,'normal',false,'10.0.0.1', "
               "now() - ($10 || ' minutes')::interval)",
               {param(attemptId), param(s.userId), param(a.task.id), param(randomHex(12)),
                paramJson({{"simulated", true}}), param(a.result), param(a.score),
                param(a.timeMs), param(a.hints), param(a.minutesAgo)});

        if (a.xp > 0) {
            balance += a.xp;
            t.exec("INSERT INTO xp_events (student_id, amount, reason, source_type, source_id, "
                   "balance_after, course_id, season_id, created_at) VALUES ($1,$2,'task_completed',"
                   "'task',$3,$4,$5,$6, now() - ($7 || ' minutes')::interval)",
                   {param(s.userId), param(a.xp), param(a.task.id), param(balance), param(s.courseId),
                    seasonId.empty() ? std::nullopt : param(seasonId), param(a.minutesAgo)});
        }
        if (a.result == "correct") {
            ++correctStreak;
        } else {
            correctStreak = 0;
        }
        (void)correctStreak;

        auto& state = s.mastery[a.task.topicId];
        state = ai::MasteryModel::update(state, a.result == "correct", a.task.difficulty);

        double& best = s.taskBest[a.task.id];
        best = std::max(best, a.score);
        s.levelScores[a.task.levelId].push_back(std::max(best, a.score));
        s.levelOrder[a.task.levelId] = a.task.orderNo;
        s.levelBoss[a.task.levelId] = a.task.boss;
        s.levelXpReward[a.task.levelId] = 120;

        const int dayAgo = a.minutesAgo / (60 * 24);
        const std::string day = std::to_string(dayAgo);
        s.dailyXp[day] += a.xp;
    }

    // Streak: consecutive days with activity ending today.
    std::set<int> activeDays;
    for (const auto& [day, xp] : s.dailyXp) {
        if (xp > 0) activeDays.insert(std::stoi(day));
    }
    int streak = 0;
    int cursor = activeDays.count(0) ? 0 : (activeDays.count(1) ? 1 : -1);
    if (cursor >= 0) {
        while (activeDays.count(cursor)) {
            ++streak;
            ++cursor;
        }
    }
    s.currentStreak = streak;
    int longest = 0, run = 0, prev = -1000;
    std::vector<int> sorted(activeDays.begin(), activeDays.end());
    std::sort(sorted.begin(), sorted.end());
    for (int d : sorted) {
        run = (d == prev + 1) ? run + 1 : 1;
        longest = std::max(longest, run);
        prev = d;
    }
    s.longestStreak = std::max(longest, streak);
    s.totalXp = balance;
    s.level = XpEngine::levelForXp(balance);
    s.coins = static_cast<int>(balance / 10);
    if (!activeDays.empty()) {
        t.exec("UPDATE students SET last_activity_date = $1::date, last_active_at = now() "
               "WHERE user_id = $2",
               {param(today), param(s.userId)});
    }

    // Topic mastery rows
    for (const auto& [topicId, state] : s.mastery) {
        t.exec("INSERT INTO topic_mastery (student_id, topic_id, mastery, confidence, last_seen_at, "
               "streak_correct, streak_wrong, attempts, correct) "
               "VALUES ($1,$2,$3,$4, now(), $5, $6, $7, $8) ON CONFLICT DO NOTHING",
               {param(s.userId), param(topicId), param(std::round(state.mastery * 10000) / 10000),
                param(std::round(state.confidence * 10000) / 10000), param(state.streakCorrect),
                param(state.streakWrong), param(state.attempts), param(state.correct)});
    }

    // Level progress
    int completedLevels = 0;
    int starsTotal = 0;
    for (const auto& [levelId, scores] : s.levelScores) {
        double sum = 0;
        for (double sc : scores) sum += sc;
        const double avg = scores.empty() ? 0 : sum / scores.size();
        const bool completed = !scores.empty() && avg >= 0.6;
        const int stars = !completed ? 0 : (avg >= 0.95 ? 3 : (avg >= 0.8 ? 2 : 1));
        if (completed) {
            ++completedLevels;
            starsTotal += stars;
            const int reward = s.levelBoss[levelId] ? 180 : 120;
            balance += reward;
            t.exec("INSERT INTO xp_events (student_id, amount, reason, source_type, source_id, "
                   "balance_after, course_id) VALUES ($1,$2,'level_completed','level',$3,$4,$5)",
                   {param(s.userId), param(reward), param(levelId), param(balance), param(s.courseId)});
        }
        t.exec("INSERT INTO level_progress (student_id, level_id, status, stars, best_accuracy, "
               "xp_earned, attempts_count, completed_at) VALUES ($1,$2,$3,$4,$5,$6,$7,$8) "
               "ON CONFLICT DO NOTHING",
               {param(s.userId), param(levelId), param(completed ? "completed" : "in_progress"),
                param(stars), param(avg), param(completed ? (s.levelBoss[levelId] ? 180 : 120) : 0),
                param((int)scores.size()),
                completed ? param(isoNow())
                          : std::optional<std::string>()});
    }
    s.totalXp = balance;
    s.level = XpEngine::levelForXp(balance);

    const int totalLevels = 8;
    const double progressPct = std::min(100.0, 100.0 * completedLevels / totalLevels);
    t.exec("INSERT INTO course_enrollments (student_id, course_id, progress_pct, status) "
           "VALUES ($1,$2,$3,$4) ON CONFLICT (student_id, course_id) DO UPDATE SET "
           "progress_pct = EXCLUDED.progress_pct, status = EXCLUDED.status",
           {param(s.userId), param(s.courseId), param(progressPct),
            param(completedLevels >= totalLevels ? "completed" : "active")});

    t.exec("UPDATE students SET total_xp = $1, level = $2, coins = $3, current_streak = $4, "
           "longest_streak = $5, daily_goal_xp = $6, league = $7, updated_at = now() "
           "WHERE user_id = $8",
           {param(s.totalXp), param(s.level), param(s.coins), param(s.currentStreak),
            param(s.longestStreak), param(s.dailyGoal), param(s.league), param(s.userId)});

    // Daily goals + streak log
    for (const auto& [ago, xp] : s.dailyXp) {
        if (xp <= 0) continue;
        const int d = std::stoi(ago);
        const bool met = xp >= s.dailyGoal;
        t.exec("INSERT INTO daily_goals (student_id, goal_date, goal_xp, xp_earned, completed_at) "
               "VALUES ($1, current_date - $2::int, $3, $4, $5) ON CONFLICT DO NOTHING",
               {param(s.userId), param(d), param(s.dailyGoal), param(xp),
                met ? param(isoNow()) : std::optional<std::string>()});
        t.exec("INSERT INTO streak_log (student_id, log_date, xp_earned, goal_met) "
               "VALUES ($1, current_date - $2::int, $3, $4) ON CONFLICT DO NOTHING",
               {param(s.userId), param(d), param(xp), param(met)});
    }

    if (!seasonId.empty() && s.totalXp > 0) {
        t.exec("INSERT INTO season_progress (season_id, student_id, season_xp, tier) "
               "VALUES ($1,$2,$3::bigint,(SELECT count(*) FROM season_rewards WHERE season_id = $1 "
               "AND required_xp <= $3::bigint)) ON CONFLICT (season_id, student_id) DO UPDATE SET "
               "season_xp = EXCLUDED.season_xp, tier = EXCLUDED.tier",
               {param(seasonId), param(s.userId), param(s.totalXp)});
    }
}

void seedTeams(Tx& t, const std::vector<std::string>& studentIds,
               const std::vector<std::string>& courseIds, const std::string& seasonId) {
    if (t.scalarInt("SELECT count(*) FROM teams") > 0) return;
    struct TeamSpec {
        const char* name;
        const char* emblem;
    };
    const TeamSpec specs[6] = {{"Р С’Р В»РўвЂњРЎвЂ№РЎР‚Р В»Р В°РЎР‚", "bolt"},   {"Р РЋР В°Р СРЎС“РЎР‚РЎвЂ№РўвЂє", "forest"},
                               {"Р В¦Р С‘Р С”Р В»Р С•Р Р…", "cyclone"},  {"Р СњР ВµР в„–РЎР‚Р С•Р Р…", "psychology"},
                               {"Р С’Р В»РЎвЂљРЎвЂ№Р Р… РўвЂєР В°Р Р…Р В°РЎвЂљ", "flutter_dash"}, {"Р С™Р Р†Р В°Р Р…РЎвЂљ", "science"}};
    std::vector<std::string> teamIds;
    for (int i = 0; i < 6; ++i) {
        const std::string id = uuidV4();
        teamIds.push_back(id);
        std::string code = "LUMINA" + std::to_string(i + 1);
        t.exec("INSERT INTO teams (id, name, emblem, description, captain_id, invite_code) "
               "VALUES ($1,$2,$3,$4,$5,$6)",
               {param(id), param(specs[i].name), param(specs[i].emblem),
                param("Р вЂќР С•РЎРѓРЎвЂљРЎвЂ№РўвЂє РЎвЂљРЎС“РЎР‚Р Р…Р С‘РЎР‚РЎвЂ“Р Р…Р Вµ Р В°РЎР‚Р Р…Р В°Р В»РўвЂњР В°Р Р… Р С”Р С•Р СР В°Р Р…Р Т‘Р В°"), param(studentIds[i % studentIds.size()]),
                param(code)});
    }
    for (size_t i = 0; i < studentIds.size() && i < 30; ++i) {
        const std::string teamId = teamIds[i % 6];
        t.exec("INSERT INTO team_members (team_id, student_id, role) VALUES ($1,$2,$3) "
               "ON CONFLICT DO NOTHING",
               {param(teamId), param(studentIds[i]), param(i < 6 ? "captain" : "member")});
    }

    const std::string tournamentId = uuidV4();
    t.exec("INSERT INTO tournaments (id, title, description, metric, starts_at, ends_at, status, rules) "
           "VALUES ($1,$2::jsonb,$3::jsonb,'xp', now() - interval '5 days', now() + interval '9 days', "
           "'active', $4::jsonb)",
           {param(tournamentId),
            paramJson({{"kk", "Р С™Р С•Р СР В°Р Р…Р Т‘Р В°Р В»РЎвЂ№РўвЂє РЎвЂљРЎС“РЎР‚Р Р…Р С‘РЎР‚: Р С™РўР‡Р В·Р С–РЎвЂ“ Р С”РЎС“Р В±Р С•Р С”"},
                       {"ru", "Р С™Р С•Р СР В°Р Р…Р Т‘Р Р…РЎвЂ№Р в„– РЎвЂљРЎС“РЎР‚Р Р…Р С‘РЎР‚: Р С›РЎРѓР ВµР Р…Р Р…Р С‘Р в„– Р С”РЎС“Р В±Р С•Р С”"},
                       {"en", "Team Tournament: Autumn Cup"}}),
            paramJson({{"kk", "Р вЂўРўР€ Р С”РЈВ©Р С— XP Р В¶Р С‘Р Р…Р В°РўвЂњР В°Р Р… Р С”Р С•Р СР В°Р Р…Р Т‘Р В° Р В¶Р ВµРўР€Р ВµР Т‘РЎвЂ“."},
                       {"ru", "Р СџР С•Р В±Р ВµР В¶Р Т‘Р В°Р ВµРЎвЂљ Р С”Р С•Р СР В°Р Р…Р Т‘Р В° РЎРѓ Р Р…Р В°Р С‘Р В±Р С•Р В»РЎРЉРЎв‚¬Р С‘Р С XP."},
                       {"en", "The team with the most XP wins."}}),
            paramJson({{"metric", "xp"}, {"period", "tournament"}})});
    for (const auto& teamId : teamIds) {
        t.exec("INSERT INTO tournament_teams (tournament_id, team_id, score) "
               "SELECT $1, $2, coalesce(sum(s.total_xp), 0) FROM team_members tm "
               "JOIN students s ON s.user_id = tm.student_id WHERE tm.team_id = $2",
               {param(tournamentId), param(teamId)});
    }
    t.exec("WITH ranked AS (SELECT tournament_id, team_id, row_number() OVER (PARTITION BY tournament_id "
           "ORDER BY score DESC) r FROM tournament_teams) UPDATE tournament_teams tt SET rank = ranked.r "
           "FROM ranked WHERE tt.tournament_id = ranked.tournament_id AND tt.team_id = ranked.team_id");
    (void)courseIds;
    (void)seasonId;
}

} // namespace

int main(int argc, char** argv) {
    const bool reset = argc > 1 && std::string(argv[1]) == "--reset";
    try {
        Config::instance().load();
        Log::init("info", "logs/seeder.log");
        Crypto::init();
        Db::instance().init(Config::instance().databaseUrl(), 4);

        const bool demoMode = Config::instance().getBool("DEMO_MODE", true);
        const std::string bootstrapAdminEmail = Config::instance().get("BOOTSTRAP_ADMIN_EMAIL");
        const std::string bootstrapAdminPassword = Config::instance().get("BOOTSTRAP_ADMIN_PASSWORD");
        if (!demoMode && (bootstrapAdminEmail.empty() || bootstrapAdminPassword.size() < 12)) {
            throw std::runtime_error(
                "DEMO_MODE=0 requires BOOTSTRAP_ADMIN_EMAIL and a BOOTSTRAP_ADMIN_PASSWORD "
                "with at least 12 characters");
        }

        const fs::path seeds = Config::instance().seedsDir();
        Log::get()->info("seeding from {}", fs::absolute(seeds).string());

        if (reset) {
            Log::get()->warn("truncating all data (--reset)");
            Db::instance().tx([](Tx& t) {
                t.execRaw("TRUNCATE TABLE ai_tips, ai_decisions, topic_mastery, streak_log, "
                          "student_quests, daily_goals, anticheat_flags, notifications, inventory, "
                          "purchases, shop_items, tournament_teams, tournaments, team_members, teams, "
                          "leaderboards, season_progress, season_rewards, seasons, student_achievements, "
                          "achievements, xp_events, level_progress, attempts, tasks, levels, topics, "
                          "course_enrollments, courses, daily_quests, ai_tip_templates, refresh_tokens, "
                          "audit_log, students, users RESTART IDENTITY CASCADE");
            });
        }

        const json usersDoc = readJson(seeds / "demo" / "users.json");
        json achievements = readJson(seeds / "achievements" / "achievements.json");
        json shop = readJson(seeds / "shop" / "items.json");
        json quests = readJson(seeds / "quests" / "quests.json");
        json tips = readJson(seeds / "ai_tips.json");

        std::vector<json> courses;
        for (const auto& entry : fs::directory_iterator(seeds / "courses")) {
            if (entry.path().extension() == ".json") courses.push_back(readJson(entry.path()));
        }
        std::sort(courses.begin(), courses.end(), [](const json& a, const json& b) {
            return a.value("slug", "") < b.value("slug", "");
        });

        const std::string passwordHash = Crypto::hashPassword(
            demoMode ? std::string(kPassword) : bootstrapAdminPassword);

        std::string teacherId;
        std::string seasonId;
        std::string today;
        DemoStudent dummy;
        (void)dummy;

        Db::instance().tx([&](Tx& t) {
            if (demoMode) {
                for (const auto& teacher : usersDoc["teachers"]) {
                    if (!loadUserByEmail(t, teacher["email"].get<std::string>()).empty()) {
                        teacherId = loadUserByEmail(t, teacher["email"].get<std::string>());
                        break;
                    }
                    const std::string id = createUser(t, teacher["email"], teacher["name"], "teacher",
                                                      teacher.value("locale", "kk"), passwordHash,
                                                      teacher["name"], "school");
                    if (teacherId.empty()) teacherId = id;
                }
                if (loadUserByEmail(t, usersDoc["admin"]["email"].get<std::string>()).empty()) {
                    createUser(t, usersDoc["admin"]["email"], usersDoc["admin"]["name"], "admin", "kk",
                               passwordHash, "admin", "admin_panel_settings");
                }
            } else {
                teacherId = loadUserByEmail(t, bootstrapAdminEmail);
                if (teacherId.empty()) {
                    teacherId = createUser(t, bootstrapAdminEmail, "Lumina Admin", "admin", "kk",
                                           passwordHash, "admin", "admin_panel_settings");
                }
            }
        });

        Db::instance().tx([&](Tx& t) {
            for (const auto& course : courses) seedCourse(t, course, teacherId);
            seedAchievements(t, achievements);
            seedShop(t, shop);
            seedQuests(t, quests);
            seedTips(t, tips);
            seedSeasons(t, teacherId);
            seasonId = t.scalar("SELECT id::text FROM seasons WHERE status = 'active' LIMIT 1")
                           .value_or("");
            today = t.scalar("SELECT (now() AT TIME ZONE 'Asia/Qyzylorda')::date::text").value_or("");
        });

        std::vector<std::string> demoStudentIds;
        if (demoMode) {
            Db::instance().tx([&](Tx& t) {
                for (const auto& s : usersDoc["students"]) {
                    const std::string email = s["email"];
                    std::string id = loadUserByEmail(t, email);
                    if (id.empty()) {
                        id = createUser(t, email, s["name"], "student", s.value("locale", "kk"),
                                        passwordHash, s["nickname"], s.value("avatar", "face"));
                    }
                    demoStudentIds.push_back(id);
                }
            });
        }

        // ---------------- history ----------------
        if (demoMode) {
            const std::string marker = "simulated";
            int attemptsAlready = static_cast<int>(Db::instance().tx([&](Tx& t) {
                return t.scalarInt("SELECT count(*) FROM attempts WHERE answer ? $1", {param(marker)});
            }));
            if (attemptsAlready > 0) {
                Log::get()->info("demo history already present ({} attempts), skipping", attemptsAlready);
            } else {
                struct Profile {
                    double accuracy;
                    int maxLevels;
                    int attemptsPerTask;
                    const char* league;
                };
                std::map<std::string, Profile> profiles = {
                    {"excellent", {0.93, 8, 2, "diamond"}},
                    {"steady", {0.76, 6, 2, "gold"}},
                    {"irregular", {0.67, 5, 2, "silver"}},
                    {"stuck", {0.46, 4, 3, "bronze"}}};
                std::vector<json> courseRows = Db::instance().query(
                    "SELECT id, title FROM courses WHERE status = 'published' ORDER BY sort_order, slug");
                std::mt19937 rng(20261029u);
                std::uniform_real_distribution<double> uni(0.0, 1.0);

                std::vector<DemoStudent> sims;
                size_t idx = 0;
                for (const auto& s : usersDoc["students"]) {
                    if (idx >= demoStudentIds.size() || courseRows.empty()) break;
                    DemoStudent sim;
                    sim.userId = demoStudentIds[idx];
                    sim.nickname = s["nickname"];
                    sim.profile = s.value("profile", "steady");
                    const auto& course = courseRows[idx % courseRows.size()];
                    sim.courseId = course["id"];
                    sim.courseTitle = I18n::pick(course["title"], "kk");
                    sim.dailyGoal = s.value("daily_goal_xp", 50);
                    const Profile prof = profiles.count(sim.profile) ? profiles[sim.profile]
                                                                      : profiles["steady"];
                    sim.league = prof.league;

                    auto tasks = Db::instance().tx([&](Tx& t) { return loadTasks(t, sim.courseId); });
                    std::vector<TaskInfo> pool;
                    for (const auto& task : tasks) {
                        if (task.orderNo <= prof.maxLevels) pool.push_back(task);
                    }
                    if (pool.empty()) pool = tasks;
                    if (pool.empty()) {
                        ++idx;
                        continue;
                    }
                    const int attempts = std::clamp(
                        static_cast<int>(pool.size()) * prof.attemptsPerTask + 8, 20, 90);
                    double streakCorrect = 0;
                    for (int i = 0; i < attempts; ++i) {
                        SimAttempt a;
                        a.task = pool[rng() % pool.size()];
                        a.hints = (rng() % 4 == 0) ? 1 + static_cast<int>(rng() % 2) : 0;
                        const double pid = static_cast<double>(i) / std::max(1, attempts - 1);
                        a.minutesAgo = static_cast<int>((1.0 - pid) * 60 * 24 * 29) + 20;
                        double p = prof.accuracy - (a.task.difficulty - 4) * 0.02 - a.hints * 0.03;
                        p = std::clamp(p, 0.12, 0.97);
                        if (uni(rng) < p) {
                            a.score = 1.0;
                            a.result = "correct";
                            streakCorrect += 1;
                        } else if (uni(rng) < 0.25) {
                            a.score = 0.5;
                            a.result = "partial";
                            streakCorrect = 0;
                        } else {
                            a.score = 0.0;
                            a.result = "wrong";
                            streakCorrect = 0;
                        }
                        a.timeMs = 4000 + static_cast<int>(uni(rng) * a.task.timeLimit * 700);
                        XpInput xin;
                        xin.baseXp = a.task.baseXp;
                        xin.difficulty = a.task.difficulty;
                        xin.score = a.score;
                        xin.timeSpentMs = a.timeMs;
                        xin.timeLimitSec = a.task.timeLimit;
                        xin.hintCount = a.hints;
                        xin.streak = static_cast<int>(std::min(10.0, streakCorrect));
                        xin.repeated = i > static_cast<int>(pool.size());
                        a.xp = XpEngine::compute(xin).amount;
                        sim.attempts.push_back(a);
                    }
                    sims.push_back(std::move(sim));
                    ++idx;
                }

                for (auto& sim : sims) {
                    Db::instance().tx([&](Tx& t) {
                        simulateAndInsert(t, sim, seasonId, today);
                    });
                }

                std::vector<std::string> ids;
                for (const auto& s : sims) ids.push_back(s.userId);
                Db::instance().tx([&](Tx& t) {
                    seedTeams(t, ids, {}, seasonId);
                });

                Log::get()->info("demo history generated for {} students", sims.size());
            }
        }

        // Achievements: evaluate for each demo student (all event buckets).
        for (const auto& id : demoStudentIds) {
            for (const char* event : {"task_completed", "level_completed", "streak_updated",
                                      "quest_completed"}) {
                try {
                    Db::instance().tx([&](Tx& t) {
                        AchievementService::evaluate(t, id, "kk", event);
                    });
                } catch (const std::exception& e) {
                    Log::get()->warn("achievement eval failed: {}", e.what());
                }
            }
        }

        Db::instance().tx([](Tx& t) {
            LeaderboardService::snapshot("global", "all", "", "");
            LeaderboardService::snapshot("weekly", "week", "", "");
            (void)t;
        });

        if (demoMode) {
            Log::get()->info("seed complete. demo login: student alisher@lumina.kz / Lumina2026!");
        } else {
            Log::get()->info("seed complete. demo accounts are disabled; bootstrap admin is ready");
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "seed failed: " << e.what() << "\n";
        return 1;
    }
}



