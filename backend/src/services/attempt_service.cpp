#include "services/attempt_service.h"
#include "ai/mastery_model.h"
#include "ai/tip_renderer.h"
#include "core/config.h"
#include "core/crypto.h"
#include "core/i18n.h"
#include "core/log.h"
#include "core/util.h"
#include "sandbox/sandbox.h"
#include "services/achievement_service.h"
#include "services/course_service.h"
#include "services/grading.h"
#include "services/quest_service.h"
#include "services/xp_engine.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace lumina {

namespace {

int asInt(const json& v, int def = 0) {
    if (v.is_number_integer()) return v.get<int>();
    if (v.is_number()) return static_cast<int>(v.get<double>());
    if (v.is_string()) {
        try {
            return std::stoi(v.get<std::string>());
        } catch (...) {
        }
    }
    return def;
}

double asDouble(const json& v, double def = 0.0) {
    if (v.is_number()) return v.get<double>();
    if (v.is_string()) {
        try {
            return std::stod(v.get<std::string>());
        } catch (...) {
        }
    }
    return def;
}

json localizeAnswerKey(const json& key, const std::string& locale) {
    if (!key.is_object()) return key;
    json out = json::object();
    for (auto it = key.begin(); it != key.end(); ++it) {
        if (it.value().is_object() && (it.value().contains("kk") || it.value().contains("ru"))) {
            out[it.key()] = I18n::pick(it.value(), locale);
        } else if (it.value().is_array()) {
            json arr = json::array();
            for (const auto& v : it.value()) arr.push_back(localizeAnswerKey(v, locale));
            out[it.key()] = arr;
        } else {
            out[it.key()] = it.value();
        }
    }
    return out;
}

int currentCombo(Tx& t, const std::string& studentId) {
    auto rows = t.query("SELECT result FROM attempts WHERE student_id = $1 "
                        "ORDER BY created_at DESC LIMIT 20",
                        {param(studentId)});
    int combo = 0;
    for (const auto& r : rows) {
        if (r.value("result", "") == "correct") ++combo;
        else break;
    }
    return combo;
}

json sandboxDetails(const SandboxResult& res) {
    json cases = json::array();
    for (const auto& c : res.cases) {
        cases.push_back({{"index", c.index},
                         {"passed", c.passed},
                         {"output", c.output.substr(0, 500)},
                         {"error", c.error.substr(0, 500)},
                         {"timed_out", c.timedOut}});
    }
    return {{"compiled", res.compiled},
            {"compile_error", res.compileError.substr(0, 4000)},
            {"passed", res.passed},
            {"total", res.total},
            {"cases", cases},
            {"sandbox_unavailable", res.sandboxUnavailable}};
}

} // namespace

json AttemptService::taskForStudent(const AuthCtx& ctx, const std::string& taskId) {
    return Db::instance().tx([&](Tx& t) {
        auto task = t.one("SELECT * FROM tasks WHERE id = $1 AND status = 'published'",
                          {param(taskId)});
        if (task.is_null()) throw notFound("Task not found");
        json out = Grading::sanitizeTask(task, ctx.locale);

        auto info = t.one(
            "SELECT count(*) AS attempts, coalesce(max(score), 0) AS best_score "
            "FROM attempts WHERE task_id = $1 AND student_id = $2",
            {param(taskId), param(ctx.userId)});
        out["my_attempts"] = info.value("attempts", 0);
        out["best_score"] = asDouble(info.value("best_score", json(0.0)));

        auto topic = t.one("SELECT code, name FROM topics WHERE id = $1",
                           {param(task["topic_id"].get<std::string>())});
        if (!topic.is_null()) {
            out["topic"] = {{"code", topic["code"]},
                            {"name", I18n::pick(topic["name"], ctx.locale)}};
        }

        // Hint packs purchased in the shop grant extra free hints.
        auto packs = t.scalarInt(
            "SELECT coalesce(sum(i.qty), 0) FROM inventory i JOIN shop_items s ON s.id = i.item_id "
            "WHERE i.student_id = $1 AND s.kind = 'hint_pack'",
            {param(ctx.userId)});
        out["extra_hints"] = packs;
        return out;
    });
}

json AttemptService::hint(const AuthCtx& ctx, const std::string& taskId, const json& body) {
    return Db::instance().tx([&](Tx& t) {
        auto task = t.one("SELECT hints FROM tasks WHERE id = $1", {param(taskId)});
        if (task.is_null()) throw notFound("Task not found");
        const int index = asInt(body.value("hint_index", 0));
        const auto& hints = task["hints"];
        if (index < 0 || index >= static_cast<int>(hints.size())) {
            throw badRequest("Hint index out of range");
        }
        const int penaltyPercent = 15 * (index + 1);
        return json{{"hint_index", index},
                    {"text", I18n::pick(hints[index], ctx.locale)},
                    {"penalty_percent", penaltyPercent},
                    {"hints_total", hints.size()}};
    });
}

json AttemptService::history(const AuthCtx& ctx, int limit, int offset) {
    limit = std::clamp(limit, 1, 100);
    offset = std::max(0, offset);
    return Db::instance().tx([&](Tx& t) {
        auto rows = t.query(
            "SELECT a.id, a.result, a.score, a.time_spent_ms, a.hints_used, a.context, "
            "a.suspicious, a.created_at, t.type, t.difficulty, t.prompt, tk.title AS topic_name, "
            "l.title AS level_title, c.title AS course_title "
            "FROM attempts a JOIN tasks t ON t.id = a.task_id "
            "JOIN topics tk ON tk.id = t.topic_id JOIN levels l ON l.id = t.level_id "
            "JOIN courses c ON c.id = l.course_id "
            "WHERE a.student_id = $1 ORDER BY a.created_at DESC LIMIT $2 OFFSET $3",
            {param(ctx.userId), param(limit), param(offset)});
        json out = json::array();
        for (const auto& r : rows) {
            json item = r;
            item["prompt"] = I18n::pick(r["prompt"], ctx.locale);
            item["topic_name"] = I18n::pick(r["topic_name"], ctx.locale);
            item["level_title"] = I18n::pick(r["level_title"], ctx.locale);
            item["course_title"] = I18n::pick(r["course_title"], ctx.locale);
            out.push_back(item);
        }
        const int64_t total = t.scalarInt(
            "SELECT count(*) FROM attempts WHERE student_id = $1", {param(ctx.userId)});
        return json{{"attempts", out}, {"total", total}, {"limit", limit}, {"offset", offset}};
    });
}

json AttemptService::submit(const AuthCtx& ctx, const std::string& taskId, const json& body,
                            const std::string& ip) {
    if (ctx.role != "student") throw forbidden("Only students can submit attempts");

    const std::string token = trimCopy(body.value("attempt_token", ""));
    if (token.size() < 8 || token.size() > 120) {
        throw unprocessable("attempt_token is required (8..120 chars)");
    }

    // Replay of a completed request returns the stored response (idempotency).
    auto cached = Db::instance().query(
        "SELECT response FROM attempts WHERE attempt_token = $1 AND student_id = $2",
        {param(token), param(ctx.userId)});
    if (!cached.empty() && cached[0]["response"].is_object() &&
        !cached[0]["response"].empty()) {
        return cached[0]["response"];
    }

    auto task = Db::instance().query("SELECT * FROM tasks WHERE id = $1 AND status = 'published'",
                                     {param(taskId)});
    if (task.empty()) throw notFound("Task not found");
    const json taskRow = task[0];

    // Code tasks are compiled and executed outside the DB transaction.
    SandboxResult sandbox;
    bool ranSandbox = false;
    if (taskRow.value("type", "") == "code") {
        const json answer = body.value("answer", json::object());
        const std::string source = answer.value("code", "");
        if (source.size() < 10) throw unprocessable("Code answer is empty");
        if (source.size() > 20000) throw unprocessable("Code is too large");
        std::vector<SandboxTest> tests;
        for (const auto& r : taskRow["answer_key"].value("tests", json::array())) {
            tests.push_back({r.value("input", ""), r.value("expected_output", "")});
        }
        if (tests.size() > 8) tests.resize(8);
        ranSandbox = true;
        sandbox = Sandbox::runCpp(source, tests, 3000, 256);
    }

    return Db::instance().tx([&](Tx& t) {
        // Double-submit protection inside the transaction.
        auto dup = t.one("SELECT response FROM attempts WHERE attempt_token = $1 AND student_id = $2",
                         {param(token), param(ctx.userId)});
        if (!dup.is_null() && dup["response"].is_object() && !dup["response"].empty()) {
            return dup["response"];
        }

        auto student = t.one(
            "SELECT user_id, nickname, avatar, total_xp, level, coins, current_streak, "
            "longest_streak, last_activity_date, daily_goal_xp FROM students "
            "WHERE user_id = $1 FOR UPDATE",
            {param(ctx.userId)});
        if (student.is_null()) throw notFound("Student profile not found");

        // Reload the task inside the transaction (versioning safety).
        auto taskRow2 = t.one("SELECT * FROM tasks WHERE id = $1", {param(taskId)});
        if (taskRow2.is_null()) throw notFound("Task not found");
        auto level = t.one("SELECT * FROM levels WHERE id = $1",
                           {param(taskRow2["level_id"].get<std::string>())});
        if (level.is_null()) throw notFound("Level not found");
        const std::string courseId = level["course_id"].get<std::string>();
        const std::string levelId = level["id"].get<std::string>();
        const std::string topicId = taskRow2["topic_id"].get<std::string>();

        svc::ensureEnrollment(t, ctx.userId, courseId);

        const std::string today = svc::todayLocal(t);
        const int todayAttempts = static_cast<int>(t.scalarInt(
            "SELECT count(*) FROM attempts WHERE student_id = $1 AND task_id = $2 "
            "AND (created_at AT TIME ZONE 'Asia/Qyzylorda')::date = $3::date",
            {param(ctx.userId), param(taskId), param(today)}));
        if (todayAttempts >= 6) {
            throw tooMany("Attempt limit for this task reached (6 per day)");
        }
        if (todayAttempts > 0) {
            auto secondsSince = t.scalarInt(
                "SELECT extract(epoch FROM now() - max(created_at))::int FROM attempts "
                "WHERE student_id = $1 AND task_id = $2",
                {param(ctx.userId), param(taskId)});
            if (secondsSince < 15) {
                throw ApiError{429, "cooldown",
                               "Please wait before retrying this task",
                               {{"retry_after", 15 - secondsSince}}};
            }
        }

        const json answer = body.value("answer", json::object());
        if (answer.empty()) throw unprocessable("Answer is empty");

        const int timeLimitSec = taskRow2.value("time_limit_sec", 120);
        const int timeSpent = std::clamp(asInt(body.value("time_spent_ms", 0)), 0,
                                         timeLimitSec * 1000 * 3);
        const int hintsTotal = static_cast<int>(taskRow2["hints"].size());
        const int hintsUsed = std::clamp(asInt(body.value("hints_used", 0)), 0, hintsTotal);
        std::string context = body.value("context", "normal");
        static const std::set<std::string> contexts = {"normal", "duel", "tournament", "season",
                                                       "daily"};
        if (!contexts.count(context)) context = "normal";

        // ---- grading ----
        GradeResult grade;
        json gradingDetails = json::object();
        if (taskRow2.value("type", "") == "code") {
            grade.needsSandbox = false;
            grade.score = sandbox.total > 0
                              ? static_cast<double>(sandbox.passed) / sandbox.total
                              : 0.0;
            grade.result = grade.score >= 0.999 ? "correct"
                                                : (grade.score > 0 ? "partial" : "wrong");
            gradingDetails = sandboxDetails(sandbox);
        } else {
            grade = Grading::grade(taskRow2["type"].get<std::string>(),
                                   taskRow2["payload"], taskRow2["answer_key"], answer);
            gradingDetails = grade.details;
        }

        // ---- anticheat ----
        bool suspicious = false;
        std::string suspiciousReason;
        const std::string type = taskRow2.value("type", "");
        if (type != "timed_quiz" && grade.result == "correct" && timeSpent < 1200) {
            suspicious = true;
            suspiciousReason = "too_fast";
        }
        if (timeSpent > timeLimitSec * 1000 * 2) {
            suspicious = true;
            suspiciousReason = "time_exceeded";
        }
        const std::string answerHash = Crypto::sha256Hex(answer.dump());
        const int64_t sameAnswers = t.scalarInt(
            "SELECT count(DISTINCT a.student_id) FROM attempts a "
            "JOIN tasks tk ON tk.id = a.task_id "
            "WHERE a.task_id = $1 AND a.student_id <> $2 AND a.created_at > now() - interval '15 minutes' "
            "AND a.answer = $3::jsonb",
            {param(taskId), param(ctx.userId), paramJson(answer)});
        if (sameAnswers >= 4) {
            suspicious = true;
            suspiciousReason = "mass_identical_answers";
        }
        (void)answerHash;

        const bool repeated = t.scalarInt(
                                   "SELECT count(*) FROM attempts WHERE student_id = $1 "
                                   "AND task_id = $2",
                                   {param(ctx.userId), param(taskId)}) > 0;
        const int combo = currentCombo(t, ctx.userId);

        XpInput xpIn;
        xpIn.baseXp = taskRow2.value("base_xp", 30);
        xpIn.difficulty = taskRow2.value("difficulty", 1);
        xpIn.score = grade.score;
        xpIn.timeSpentMs = timeSpent;
        xpIn.timeLimitSec = timeLimitSec;
        xpIn.hintCount = hintsUsed;
        xpIn.streak = combo;
        xpIn.repeated = repeated;
        xpIn.suspicious = suspicious;
        const XpBreakdown xp = XpEngine::compute(xpIn);

        const std::string attemptId = uuidV4();
        t.exec(
            "INSERT INTO attempts (id, student_id, task_id, attempt_token, answer, result, score, "
            "time_spent_ms, hints_used, context, suspicious, ip) "
            "VALUES ($1, $2, $3, $4, $5::jsonb, $6, $7, $8, $9, $10, $11, $12)",
            {param(attemptId), param(ctx.userId), param(taskId), param(token), paramJson(answer),
             param(grade.result), param(grade.score), param(timeSpent), param(hintsUsed),
             param(context), param(suspicious), param(ip)});

        if (suspicious) {
            t.exec("INSERT INTO anticheat_flags (id, student_id, attempt_id, kind, severity, details) "
                   "VALUES ($1, $2, $3, $4, $5, $6::jsonb)",
                   {param(uuidV4()), param(ctx.userId), param(attemptId), param(suspiciousReason),
                    param(2), paramJson({{"time_spent_ms", timeSpent},
                                         {"result", grade.result},
                                         {"same_answers", sameAnswers}})});
        }

        // ---- XP + progression ----
        long long balance = student.value("total_xp", 0LL);
        const std::string seasonId = svc::activeSeasonId(t);
        int xpGained = xp.amount;
        if (xpGained > 0) {
            balance = svc::addXp(t, ctx.userId, xpGained, "task_completed", "task", taskId,
                                 courseId, seasonId, balance);
        }

        // Daily goal
        const int goalXp = student.value("daily_goal_xp", 50);
        auto goal = t.one(
            "INSERT INTO daily_goals (student_id, goal_date, goal_xp, xp_earned) "
            "VALUES ($1, $2::date, $3, $4) "
            "ON CONFLICT (student_id, goal_date) DO UPDATE "
            "SET xp_earned = daily_goals.xp_earned + EXCLUDED.xp_earned, "
            "goal_xp = EXCLUDED.goal_xp "
            "RETURNING goal_xp, xp_earned, completed_at",
            {param(ctx.userId), param(today), param(goalXp), param(xpGained)});
        bool goalCompletedNow = false;
        int goalBonus = 0;
        if (goal["completed_at"].is_null() && goal.value("xp_earned", 0) >= goal.value("goal_xp", 50)) {
            goalCompletedNow = true;
            goalBonus = 25;
            balance = svc::addXp(t, ctx.userId, goalBonus, "daily_goal", "goal", today, courseId,
                                 seasonId, balance);
            t.exec("UPDATE daily_goals SET completed_at = now() "
                   "WHERE student_id = $1 AND goal_date = $2::date",
                   {param(ctx.userId), param(today)});
        }

        // Level progress (stars + completion bonus)
        json levelProgress = CourseService::recomputeLevelProgress(t, ctx.userId, levelId, ctx.locale);
        int levelBonus = levelProgress.value("bonus_xp", 0);
        if (levelBonus > 0) {
            balance = svc::addXp(t, ctx.userId, levelBonus, "level_completed", "level", levelId,
                                 courseId, seasonId, balance);
        }

        // Streak (activity days)
        int streak = student.value("current_streak", 0);
        int longest = student.value("longest_streak", 0);
        bool streakIncreased = false;
        if (xpGained > 0) {
            const std::string lastDate = student["last_activity_date"].is_null()
                                             ? ""
                                             : student["last_activity_date"].get<std::string>();
            if (lastDate.empty()) {
                streak = 1;
                streakIncreased = true;
            } else if (lastDate == today) {
                // same day, no change
            } else {
                const int diff = t.scalarInt("SELECT ($1::date - $2::date)", {param(today), param(lastDate)});
                streak = diff == 1 ? streak + 1 : 1;
                streakIncreased = true;
            }
            longest = std::max(longest, streak);
        }

        const int newLevel = XpEngine::levelForXp(balance);
        const bool levelUp = newLevel > student.value("level", 1);
        const int coinsEarned = xpGained / 10;

        t.exec("UPDATE students SET total_xp = $1, level = $2, coins = coins + $3, "
               "current_streak = $4, longest_streak = $5, last_activity_date = $6::date, "
               "last_active_at = now(), updated_at = now() WHERE user_id = $7",
               {param(balance), param(newLevel), param(coinsEarned), param(streak), param(longest),
                param(today), param(ctx.userId)});

        t.exec("INSERT INTO streak_log (student_id, log_date, xp_earned, goal_met) "
               "VALUES ($1, $2::date, $3, $4) ON CONFLICT (student_id, log_date) DO UPDATE "
               "SET xp_earned = streak_log.xp_earned + EXCLUDED.xp_earned, "
               "goal_met = streak_log.goal_met OR EXCLUDED.goal_met",
               {param(ctx.userId), param(today), param(xpGained),
                param(!goal["completed_at"].is_null() || goalCompletedNow)});

        if (!seasonId.empty()) {
            t.exec("INSERT INTO season_progress (season_id, student_id, season_xp) "
                   "VALUES ($1, $2, $3) ON CONFLICT (season_id, student_id) DO UPDATE "
                   "SET season_xp = season_progress.season_xp + EXCLUDED.season_xp, "
                   "updated_at = now()",
                   {param(seasonId), param(ctx.userId), param(xpGained + goalBonus + levelBonus)});
        }

        // Topic mastery (adaptive engine state)
        auto masteryRow = t.one(
            "SELECT mastery, confidence, attempts, correct, streak_correct, streak_wrong "
            "FROM topic_mastery WHERE student_id = $1 AND topic_id = $2",
            {param(ctx.userId), param(topicId)});
        ai::MasteryState state;
        if (!masteryRow.is_null()) {
            state.mastery = asDouble(masteryRow["mastery"], 0.3);
            state.confidence = asDouble(masteryRow["confidence"], 0.1);
            state.attempts = masteryRow.value("attempts", 0);
            state.correct = masteryRow.value("correct", 0);
            state.streakCorrect = masteryRow.value("streak_correct", 0);
            state.streakWrong = masteryRow.value("streak_wrong", 0);
        }
        const bool correct = grade.result == "correct";
        auto updated = ai::MasteryModel::update(state, correct, taskRow2.value("difficulty", 1));
        t.exec(
            "INSERT INTO topic_mastery (student_id, topic_id, mastery, confidence, last_seen_at, "
            "streak_correct, streak_wrong, attempts, correct) "
            "VALUES ($1, $2, $3, $4, now(), $5, $6, $7, $8) "
            "ON CONFLICT (student_id, topic_id) DO UPDATE SET mastery = EXCLUDED.mastery, "
            "confidence = EXCLUDED.confidence, last_seen_at = now(), "
            "streak_correct = EXCLUDED.streak_correct, streak_wrong = EXCLUDED.streak_wrong, "
            "attempts = EXCLUDED.attempts, correct = EXCLUDED.correct, updated_at = now()",
            {param(ctx.userId), param(topicId), param(std::round(updated.mastery * 10000) / 10000),
             param(std::round(updated.confidence * 10000) / 10000), param(updated.streakCorrect),
             param(updated.streakWrong), param(updated.attempts), param(updated.correct)});

        // Quests + achievements
        QuestService::refresh(t, ctx.userId);
        json achievements = AchievementService::evaluate(t, ctx.userId, ctx.locale, "task_completed");
        if (levelProgress.value("completed", false)) {
            auto levelAch =
                AchievementService::evaluate(t, ctx.userId, ctx.locale, "level_completed");
            for (const auto& a : levelAch) achievements.push_back(a);
        }
        if (streakIncreased) {
            auto streakAch =
                AchievementService::evaluate(t, ctx.userId, ctx.locale, "streak_updated");
            for (const auto& a : streakAch) achievements.push_back(a);
        }

        // Notifications for level up / completion
        if (levelUp) {
            const std::string title = XpEngine::titleForLevel(newLevel, ctx.locale);
            svc::notify(t, ctx.userId, "level_up",
                        json{{"kk", "Р–Р°ТЈР° РґРµТЈРіРµР№!"}, {"ru", "РќРѕРІС‹Р№ СѓСЂРѕРІРµРЅСЊ!"},
                             {"en", "Level up!"}},
                        json{{"kk", "РЎС–Р· " + std::to_string(newLevel) + "-РґРµТЈРіРµР№РіРµ РєУ©С‚РµСЂС–Р»РґС–ТЈС–Р·: " + title},
                             {"ru", "Р’С‹ РґРѕСЃС‚РёРіР»Рё СѓСЂРѕРІРЅСЏ " + std::to_string(newLevel) + ": " + title},
                             {"en", "You reached level " + std::to_string(newLevel) + ": " + title}},
                        {{"level", newLevel}, {"title", title}});
        }

        // Next task suggestion within the level
        auto nextTask = t.one(
            "SELECT tk.id FROM tasks tk WHERE tk.level_id = $1 AND tk.status = 'published' "
            "AND tk.id <> $3 AND NOT EXISTS (SELECT 1 FROM attempts a WHERE a.task_id = tk.id "
            "AND a.student_id = $2 AND a.result = 'correct') "
            "ORDER BY tk.sort_order LIMIT 1",
            {param(levelId), param(ctx.userId), param(taskId)});

        json response = {
            {"attempt_id", attemptId},
            {"attempt_token", token},
            {"result", grade.result},
            {"score", grade.score},
            {"suspicious", suspicious},
            {"time_spent_ms", timeSpent},
            {"hints_used", hintsUsed},
            {"grading", gradingDetails},
            {"xp", {{"amount", xpGained},
                    {"breakdown", xp.toJson()},
                    {"total_xp", balance},
                    {"level", newLevel},
                    {"level_up", levelUp},
                    {"title", XpEngine::titleForLevel(newLevel, ctx.locale)},
                    {"coins_earned", coinsEarned}}},
            {"combo", combo + (correct ? 1 : 0)},
            {"streak", {{"current", streak}, {"longest", longest},
                        {"increased", streakIncreased}}},
            {"daily_goal", {{"goal_xp", goal.value("goal_xp", goalXp)},
                            {"xp_earned", goal.value("xp_earned", 0)},
                            {"completed", goalCompletedNow || !goal["completed_at"].is_null()},
                            {"bonus_xp", goalBonus}}},
            {"level", levelProgress},
            {"achievements", achievements},
            {"next_task_id", nextTask.is_null() ? json(nullptr) : nextTask["id"]},
            {"explanation", I18n::pick(taskRow2["explanation"], ctx.locale)},
            {"answer_key", localizeAnswerKey(taskRow2["answer_key"], ctx.locale)},
        };

        t.exec("UPDATE attempts SET response = $1::jsonb WHERE id = $2",
               {paramJson(response), param(attemptId)});
        if (levelProgress.value("completed", true) && nextTask.is_null()) {
            response["level_finished"] = true;
        }
        return response;
    });
}

} // namespace lumina

