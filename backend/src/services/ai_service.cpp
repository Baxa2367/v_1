#include "services/ai_service.h"
#include "ai/llm_client.h"
#include "ai/mastery_model.h"
#include "ai/next_task_policy.h"
#include "ai/tip_renderer.h"
#include "core/i18n.h"
#include "core/util.h"
#include "services/xp_engine.h"

#include <algorithm>
#include <cmath>
#include "services/grading.h"

namespace lumina {

namespace {

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

json localizeText(const std::string& kk, const std::string& ru, const std::string& en) {
    return {{"kk", kk}, {"ru", ru}, {"en", en}};
}

json explanationFor(const std::string& reason, const std::string& topic, double expected,
                    int errorStreak, const std::string& locale) {
    const int pct = static_cast<int>(std::lround(expected * 100));
    if (reason == "support") {
        const std::string kk = "Р‘Т±Р» С‚Р°РїСЃС‹СЂРјР°РЅС‹ В«" + topic + "В» С‚Р°Т›С‹СЂС‹Р±С‹РЅРґР°Т“С‹ Т›Р°С‚РµР»РµСЂ Т›Р°С‚Р°СЂС‹РЅР° "
                               "Р±Р°Р№Р»Р°РЅС‹СЃС‚С‹ Т±СЃС‹РЅРґС‹Т› (Т›Р°С‚Р°СЂС‹РЅР°РЅ " + std::to_string(errorStreak) +
                               " Т›Р°С‚Рµ). ТљРёС‹РЅРґС‹Т›С‚С‹ Р±С–СЂ СЃР°С‚С‹Т“Р° С‚У©РјРµРЅРґРµС‚С‚С–Рє вЂ” РјР°ТЈС‹Р·РґС‹СЃС‹ Т›Р°Р№С‚Р° СЃРµРЅС–РјРґС–Р»С–Рє Т›Т±СЂСѓ.";
        const std::string ru = "РњС‹ РїСЂРµРґР»РѕР¶РёР»Рё СЌС‚Рѕ Р·Р°РґР°РЅРёРµ, РїРѕС‚РѕРјСѓ С‡С‚Рѕ РІ С‚РµРјРµ В«" + topic +
                               "В» Р±С‹Р»Рё РѕС€РёР±РєРё РїРѕРґСЂСЏРґ (" + std::to_string(errorStreak) +
                               "). РЎР»РѕР¶РЅРѕСЃС‚СЊ СЃРЅРёР¶РµРЅР° РЅР° С€Р°Рі вЂ” СЃРЅР°С‡Р°Р»Р° РІРµСЂРЅС‘Рј СѓРІРµСЂРµРЅРЅРѕСЃС‚СЊ.";
        const std::string en = "We picked this task because of repeated mistakes in \"" + topic +
                               "\" (" + std::to_string(errorStreak) +
                               " in a row). Difficulty was lowered by one step.";
        return localizeText(kk, ru, en);
    }
    if (reason == "challenge") {
        const std::string kk = "ТљР°С‚Р°СЂС‹РЅР°РЅ 3 РґТ±СЂС‹СЃ Р¶Р°СѓР°Рї! ТљРёС‹РЅРґС‹Т›С‚С‹ Р°СЂС‚С‚С‹СЂС‹Рї, В«" + topic +
                               "В» Р±РѕР№С‹РЅС€Р° РєРµР»РµСЃС– РґРµТЈРіРµР№РіРµ У©С‚РµРјС–Р·.";
        const std::string ru = "РўСЂРё РІРµСЂРЅС‹С… РѕС‚РІРµС‚Р° РїРѕРґСЂСЏРґ! РџРѕРІС‹С€Р°РµРј СЃР»РѕР¶РЅРѕСЃС‚СЊ Рё РїРµСЂРµС…РѕРґРёРј РЅР° "
                               "СЃР»РµРґСѓСЋС‰РёР№ СѓСЂРѕРІРµРЅСЊ С‚РµРјС‹ В«" + topic + "В».";
        const std::string en = "Three correct answers in a row! Raising difficulty for \"" + topic + "\".";
        return localizeText(kk, ru, en);
    }
    if (reason == "review") {
        const std::string kk = "В«" + topic + "В» С‚Р°Т›С‹СЂС‹Р±С‹РЅ Т›Р°Р№С‚Р°Р»Р°Сѓ СѓР°Т›С‹С‚С‹ РєРµР»РґС– вЂ” Р°СЂР°Р»С‹Т› Т›Р°Р№С‚Р°Р»Р°Сѓ "
                               "РµСЃС‚Рµ СЃР°Т›С‚Р°СѓРґС‹ РЅС‹Т“Р°Р№С‚Р°РґС‹.";
        const std::string ru = "РџСЂРёС€Р»Рѕ РІСЂРµРјСЏ РїРѕРІС‚РѕСЂРёС‚СЊ В«" + topic + "В» вЂ” РёРЅС‚РµСЂРІР°Р»СЊРЅРѕРµ РїРѕРІС‚РѕСЂРµРЅРёРµ "
                               "Р·Р°РєСЂРµРїР»СЏРµС‚ РїР°РјСЏС‚СЊ.";
        const std::string en = "Time to review \"" + topic + "\" вЂ” spaced repetition strengthens memory.";
        return localizeText(kk, ru, en);
    }
    if (reason == "new_topic") {
        const std::string kk = "Р–Р°ТЈР° С‚Р°Т›С‹СЂС‹РїС‚С‹ Р·РµСЂС‚С‚РµСѓ СѓР°Т›С‹С‚С‹: В«" + topic + "В». ТљР°С‚РµР»РµСЃСѓ Т›Р°Р»С‹РїС‚С‹ вЂ” "
                               "РјР°ТЈС‹Р·РґС‹СЃС‹ ТЇР»РіС–РЅС– С‚ТЇСЃС–РЅСѓ.";
        const std::string ru = "Р’СЂРµРјСЏ РёСЃСЃР»РµРґРѕРІР°С‚СЊ РЅРѕРІСѓСЋ С‚РµРјСѓ: В«" + topic + "В». РћС€РёР±Р°С‚СЊСЃСЏ РЅРѕСЂРјР°Р»СЊРЅРѕ вЂ” "
                               "РіР»Р°РІРЅРѕРµ РїРѕРЅСЏС‚СЊ Р·Р°РєРѕРЅРѕРјРµСЂРЅРѕСЃС‚СЊ.";
        const std::string en = "Exploring a new topic: \"" + topic + "\". Mistakes are part of learning.";
        return localizeText(kk, ru, en);
    }
    const std::string kk = "Р‘Т±Р» С‚Р°РїСЃС‹СЂРјР° СЃС–Р·РґС–ТЈ РґР°РјСѓ Р°Р№РјР°Т“С‹ТЈС‹Р·РґР°: Р±РѕР»Р¶Р°РјРґС‹ С‚Р°Р±С‹СЃ РґРµТЈРіРµР№С– " +
                           std::to_string(pct) + "%. В«" + topic + "В» Р±РѕР№С‹РЅС€Р° Р±С–Р»С–РјС–ТЈС–Р·РґС– Р±РµРєС–С‚РµРјС–Р·.";
    const std::string ru = "Р­С‚Рѕ Р·Р°РґР°РЅРёРµ РІ РІР°С€РµР№ Р·РѕРЅРµ Р±Р»РёР¶Р°Р№С€РµРіРѕ СЂР°Р·РІРёС‚РёСЏ: РѕР¶РёРґР°РµРјС‹Р№ СѓСЃРїРµС… " +
                           std::to_string(pct) + "%. Р—Р°РєСЂРµРїР»СЏРµРј В«" + topic + "В».";
    const std::string en = "This task sits in your zone of proximal development: expected success " +
                           std::to_string(pct) + "%. Strengthening \"" + topic + "\".";
    (void)locale;
    return localizeText(kk, ru, en);
}

json recentStats(Tx& t, const std::string& studentId) {
    auto rows = t.query("SELECT result, time_spent_ms FROM attempts WHERE student_id = $1 "
                        "ORDER BY created_at DESC LIMIT 10",
                        {param(studentId)});
    int streakCorrect = 0, streakWrong = 0, errors = 0;
    for (const auto& r : rows) {
        if (r.value("result", "") == "correct") {
            if (streakWrong == 0) ++streakCorrect;
        } else {
            ++errors;
            if (streakCorrect == 0) ++streakWrong;
        }
    }
    return {{"streak_correct", streakCorrect},
            {"streak_wrong", streakWrong},
            {"attempts", rows.size()},
            {"errors", errors}};
}

} // namespace

json AiService::nextTask(const AuthCtx& ctx, const std::string& courseId) {
    if (ctx.role != "student") throw forbidden("Only students have an adaptive feed");
    return Db::instance().tx([&](Tx& t) {
        std::string cid = courseId;
        if (cid.empty()) {
            auto first = t.one(
                "SELECT c.id FROM course_enrollments e JOIN courses c ON c.id = e.course_id "
                "WHERE e.student_id = $1 AND e.status = 'active' ORDER BY e.updated_at DESC LIMIT 1",
                {param(ctx.userId)});
            if (first.is_null()) throw badRequest("Enroll in a course to get an adaptive task");
            cid = first["id"].get<std::string>();
        }
        auto course = t.one("SELECT id, title FROM courses WHERE id = $1", {param(cid)});
        if (course.is_null()) throw notFound("Course not found");

        auto rows = t.query(
            "SELECT tk.id, tk.topic_id, tk.difficulty, tk.base_xp, tk.generated, tp.name AS topic_name, "
            "coalesce(tm.mastery, 0.3) AS mastery, coalesce(tm.confidence, 0.1) AS confidence, "
            "coalesce(extract(day FROM now() - tm.last_seen_at), 30) AS days_since_seen, "
            "EXISTS (SELECT 1 FROM attempts a WHERE a.task_id = tk.id AND a.student_id = $2 "
            "  AND a.result = 'correct') AS solved, "
            "EXISTS (SELECT 1 FROM attempts a WHERE a.task_id = tk.id AND a.student_id = $2 "
            "  AND a.created_at > now() - interval '2 days') AS recent "
            "FROM tasks tk JOIN levels l ON l.id = tk.level_id "
            "LEFT JOIN topics tp ON tp.id = tk.topic_id "
            "LEFT JOIN topic_mastery tm ON tm.topic_id = tk.topic_id AND tm.student_id = $2 "
            "WHERE l.course_id = $1 AND tk.status = 'published'",
            {param(cid), param(ctx.userId)});
        if (rows.empty()) throw notFound("No tasks available for this course");

        std::vector<ai::Candidate> candidates;
        for (const auto& r : rows) {
            ai::Candidate c;
            c.taskId = r["id"].get<std::string>();
            c.topicId = r["topic_id"].get<std::string>();
            c.topicTitle = I18n::pick(r["topic_name"], ctx.locale);
            c.difficulty = r.value("difficulty", 1);
            c.mastery = asDouble(r["mastery"], 0.3);
            c.confidence = asDouble(r["confidence"], 0.1);
            c.daysSinceSeen = asDouble(r["days_since_seen"], 30);
            c.recent = r.value("recent", false);
            c.generated = r.value("generated", false);
            c.baseXp = r.value("base_xp", 30);
            if (r.value("solved", false) && !c.recent) continue;  // prefer unseen material
            candidates.push_back(c);
        }
        if (candidates.empty()) {
            for (const auto& r : rows) {
                ai::Candidate c;
                c.taskId = r["id"].get<std::string>();
                c.topicId = r["topic_id"].get<std::string>();
                c.topicTitle = I18n::pick(r["topic_name"], ctx.locale);
                c.difficulty = r.value("difficulty", 1);
                c.mastery = asDouble(r["mastery"], 0.3);
                c.confidence = asDouble(r["confidence"], 0.1);
                c.recent = r.value("recent", false);
                candidates.push_back(c);
            }
        }

        auto stats = recentStats(t, ctx.userId);
        auto goal = t.one("SELECT goal_xp, xp_earned FROM daily_goals WHERE student_id = $1 "
                          "AND goal_date = (now() AT TIME ZONE 'Asia/Qyzylorda')::date",
                          {param(ctx.userId)});
        ai::PolicyContext pctx;
        pctx.streakCorrect = stats.value("streak_correct", 0);
        pctx.streakWrong = stats.value("streak_wrong", 0);
        pctx.attemptsLast10 = stats.value("attempts", 0);
        pctx.errorsLast10 = stats.value("errors", 0);
        pctx.dailyGoalMet = !goal.is_null() && goal.value("xp_earned", 0) >= goal.value("goal_xp", 50);
        pctx.dailyGoalRemainingXp = goal.is_null()
                                        ? 50
                                        : std::max(0, goal.value("goal_xp", 50) - goal.value("xp_earned", 0));
        pctx.locale = ctx.locale;
        pctx.seed = static_cast<unsigned>(std::hash<std::string>{}(ctx.userId + std::to_string(unixNow() / 60)));

        auto decision = ai::NextTaskPolicy::choose(candidates, pctx);
        if (decision.empty()) throw notFound("No suitable task found");

        auto task = t.one("SELECT * FROM tasks WHERE id = $1", {param(*decision.taskId)});
        const json explanation =
            explanationFor(decision.reasonCode, decision.topicTitle, decision.expectedSuccess,
                           pctx.streakWrong, ctx.locale);

        const std::string decisionId = uuidV4();
        t.exec("INSERT INTO ai_decisions (id, student_id, course_id, task_id, reason_code, "
               "features, explanation) VALUES ($1, $2, $3, $4, $5, $6::jsonb, $7::jsonb)",
               {param(decisionId), param(ctx.userId), param(cid), param(*decision.taskId),
                param(decision.reasonCode),
                paramJson({{"mastery", decision.features.value("mastery", 0.3)},
                           {"difficulty", decision.features.value("difficulty", 1)},
                           {"expected_success", decision.expectedSuccess},
                           {"error_rate_10", pctx.attemptsLast10 > 0
                                                 ? static_cast<double>(pctx.errorsLast10) / pctx.attemptsLast10
                                                 : 0.0},
                           {"streak_correct", pctx.streakCorrect},
                           {"streak_wrong", pctx.streakWrong},
                           {"topic", decision.topicTitle}}),
                paramJson(explanation)});

        json sanitized = Grading::sanitizeTask(task, ctx.locale);
        auto topic = t.one("SELECT code, name FROM topics WHERE id = $1",
                           {param(task["topic_id"].get<std::string>())});
        sanitized["topic"] = {{"code", topic.is_null() ? "" : topic.value("code", std::string())},
                              {"name", decision.topicTitle}};
        sanitized["my_attempts"] = 0;
        sanitized["best_score"] = 0.0;
        sanitized["extra_hints"] = 0;
        return json{{"task", sanitized},
                    {"decision", {{"id", decisionId},
                                  {"reason_code", decision.reasonCode},
                                  {"reason", decision.reasonCode},
                                  {"expected_success", decision.expectedSuccess},
                                  {"topic", decision.topicTitle},
                                  {"explanation", I18n::pick(explanation, ctx.locale)},
                                  {"features", decision.features}}},
                    {"course", {{"id", cid}, {"title", I18n::pick(course["title"], ctx.locale)}}}};
    });
}

json AiService::explain(const AuthCtx& ctx, const std::string& decisionId) {
    return Db::instance().tx([&](Tx& t) {
        auto d = t.one("SELECT * FROM ai_decisions WHERE id = $1 AND student_id = $2",
                       {param(decisionId), param(ctx.userId)});
        if (d.is_null()) throw notFound("Decision not found");
        json task = nullptr;
        if (!d["task_id"].is_null()) {
            task = t.one("SELECT id, type, prompt, difficulty FROM tasks WHERE id = $1",
                         {param(d["task_id"].get<std::string>())});
            if (!task.is_null()) task["prompt"] = I18n::pick(task["prompt"], ctx.locale);
        }
        return json{{"decision_id", decisionId},
                    {"reason_code", d["reason_code"]},
                    {"explanation", I18n::pick(d["explanation"], ctx.locale)},
                    {"features", d["features"]},
                    {"task", task},
                    {"created_at", d["created_at"]}};
    });
}

json AiService::tips(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        auto stats = recentStats(t, ctx.userId);
        auto student = t.one("SELECT current_streak, daily_goal_xp, last_active_at FROM students "
                             "WHERE user_id = $1",
                             {param(ctx.userId)});
        auto goal = t.one("SELECT xp_earned FROM daily_goals WHERE student_id = $1 "
                          "AND goal_date = (now() AT TIME ZONE 'Asia/Qyzylorda')::date",
                          {param(ctx.userId)});
        auto weak = t.one(
            "SELECT tp.name, tm.mastery FROM topic_mastery tm JOIN topics tp ON tp.id = tm.topic_id "
            "WHERE tm.student_id = $1 ORDER BY tm.mastery ASC LIMIT 1",
            {param(ctx.userId)});
        const double accuracy = t.scalarDouble(
            "SELECT coalesce(avg(score) * 100, 0) FROM (SELECT score FROM attempts "
            "WHERE student_id = $1 ORDER BY created_at DESC LIMIT 20) s",
            {param(ctx.userId)});
        const int daysInactive = static_cast<int>(t.scalarInt(
            "SELECT coalesce(extract(day FROM now() - last_active_at)::int, 30) FROM students "
            "WHERE user_id = $1",
            {param(ctx.userId)}));

        const std::string topic = weak.is_null() ? "вЂ”" : I18n::pick(weak["name"], ctx.locale);
        const int streak = student.is_null() ? 0 : student.value("current_streak", 0);
        const int goalXp = student.is_null() ? 50 : student.value("daily_goal_xp", 50);
        const int earned = goal.is_null() ? 0 : goal.value("xp_earned", 0);

        std::string kind = "methodical";
        if (daysInactive >= 3 || stats.value("errors", 0) >= 6) kind = "warning";
        else if (accuracy >= 88 && streak >= 3) kind = "congratulation";
        else if (earned < goalXp) kind = "motivational";

        json vars = {{"topic", topic},
                     {"streak", streak},
                     {"accuracy", static_cast<int>(std::lround(accuracy))},
                     {"goal", goalXp},
                     {"earned", earned}};

        auto templates = t.query(
            "SELECT text FROM ai_tip_templates WHERE active AND kind = $1 "
            "ORDER BY priority DESC, code LIMIT 5",
            {param(kind)});
        std::string text;
        if (!templates.empty()) {
            const auto idx = std::hash<std::string>{}(ctx.userId + std::to_string(unixNow() / 3600)) %
                             templates.size();
            text = ai::TipRenderer::render(I18n::pick(templates[idx]["text"], ctx.locale), vars);
        } else {
            text = ai::TipRenderer::fallback(kind, ctx.locale, vars);
        }
        if (ai::LlmClient::available() && kind != "warning") {
            if (auto personalized = ai::LlmClient::personalizeTip(text, vars, ctx.locale)) {
                text = *personalized;
            }
        }

        t.exec("INSERT INTO ai_tips (id, student_id, kind, text, meta) "
               "VALUES ($1, $2, $3, $4::jsonb, $5::jsonb)",
               {param(uuidV4()), param(ctx.userId), param(kind),
                paramJson(localizeText(text, text, text)), paramJson(vars)});

        auto recent = t.query(
            "SELECT id, kind, text, created_at, read_at FROM ai_tips WHERE student_id = $1 "
            "ORDER BY created_at DESC LIMIT 5",
            {param(ctx.userId)});
        json tips = json::array();
        for (const auto& r : recent) {
            json item = r;
            item["text"] = I18n::pick(r["text"], ctx.locale);
            tips.push_back(item);
        }
        return json{{"tips", tips},
                    {"kind", kind},
                    {"signals", {{"accuracy", accuracy},
                                 {"streak", streak},
                                 {"errors_last_10", stats.value("errors", 0)},
                                 {"days_inactive", daysInactive},
                                 {"weak_topic", topic}}}};
    });
}

json AiService::forecast(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        auto student = t.one("SELECT total_xp, level, current_streak, last_active_at FROM students "
                             "WHERE user_id = $1",
                             {param(ctx.userId)});
        if (student.is_null()) throw notFound("Student not found");
        const long long xp = student.value("total_xp", 0LL);
        const int level = student.value("level", 1);
        const int toNext = XpEngine::xpToNext(level);
        long long floorXp = 0;
        for (int l = 1; l < level; ++l) floorXp += XpEngine::xpToNext(l);
        const int remaining = static_cast<int>(floorXp + toNext - xp);

        const double dailyAvg = t.scalarDouble(
            "SELECT coalesce(sum(amount)::numeric / greatest(count(DISTINCT "
            "(created_at AT TIME ZONE 'Asia/Qyzylorda')::date), 1), 0) FROM xp_events "
            "WHERE student_id = $1 AND amount > 0 AND created_at > now() - interval '14 days'",
            {param(ctx.userId)});
        const int daysInactive = static_cast<int>(t.scalarInt(
            "SELECT coalesce(extract(day FROM now() - last_active_at)::int, 30) FROM students "
            "WHERE user_id = $1",
            {param(ctx.userId)}));
        const double errorRate = t.scalarDouble(
            "SELECT coalesce(avg(CASE WHEN result = 'wrong' THEN 1 ELSE 0 END), 0) FROM "
            "(SELECT result FROM attempts WHERE student_id = $1 ORDER BY created_at DESC LIMIT 10) s",
            {param(ctx.userId)});

        const double dropout = std::clamp(0.15 + 0.10 * std::min(daysInactive, 7) + 0.35 * errorRate +
                                               (student.value("current_streak", 0) == 0 ? 0.15 : 0.0),
                                           0.0, 1.0);
        const int etaDays = dailyAvg > 1 ? static_cast<int>(std::ceil(remaining / dailyAvg)) : -1;
        const int recommendedGoal = std::clamp(static_cast<int>(std::lround(dailyAvg * 1.15)), 20, 200);

        return json{{"level", level},
                    {"total_xp", xp},
                    {"xp_remaining", remaining},
                    {"daily_avg_xp", dailyAvg},
                    {"days_to_next_level", etaDays},
                    {"dropout_risk", dropout},
                    {"days_inactive", daysInactive},
                    {"recommended_daily_goal", recommendedGoal},
                    {"message",
                     {{"kk", etaDays > 0
                                ? "РћСЃС‹ Т›Р°СЂТ›С‹РЅРјРµРЅ РєРµР»РµСЃС– РґРµТЈРіРµР№РіРµ С€Р°РјР°РјРµРЅ " + std::to_string(etaDays) + " РєТЇРЅ"
                                : "РљТЇРЅРґРµР»С–РєС‚С– РјР°Т›СЃР°С‚С‚С‹ Р±РµР»РіС–Р»РµРї, Т›Р°СЂТ›С‹РЅРґС‹ С‚Т±СЂР°Т›С‚Р°РЅРґС‹СЂС‹ТЈС‹Р·"},
                      {"ru", etaDays > 0
                                ? "РџСЂРё С‚РµРєСѓС‰РµРј С‚РµРјРїРµ СЃР»РµРґСѓСЋС‰РёР№ СѓСЂРѕРІРµРЅСЊ РїСЂРёРјРµСЂРЅРѕ С‡РµСЂРµР· " +
                                      std::to_string(etaDays) + " РґРЅ."
                                : "Р—Р°РґР°Р№С‚Рµ РґРЅРµРІРЅСѓСЋ С†РµР»СЊ, С‡С‚РѕР±С‹ СЃС‚Р°Р±РёР»РёР·РёСЂРѕРІР°С‚СЊ С‚РµРјРї"},
                      {"en", etaDays > 0
                                ? "At this pace the next level is ~" + std::to_string(etaDays) + " days away"
                                : "Set a daily goal to stabilise your pace"}}}};
    });
}

json AiService::generateTask(const AuthCtx& ctx, const json& body) {
    if (ctx.role == "student") throw forbidden("Teachers and admins only");
    const std::string topic = body.value("topic", "general");
    const int difficulty = std::clamp(body.value("difficulty", 3), 1, 10);
    if (!ai::LlmClient::available()) {
        return json{{"generated", false},
                    {"reason", "llm_unavailable"},
                    {"message", "ANTHROPIC_API_KEY is not configured; using the task bank"}};
    }
    auto task = ai::LlmClient::generateTask(topic, difficulty, ctx.locale);
    if (!task) return json{{"generated", false}, {"reason", "llm_failed"}};
    return json{{"generated", true}, {"draft", *task}, {"requires_moderation", true}};
}

json AiService::mastery(const AuthCtx& ctx, const std::string& courseId) {
    return Db::instance().tx([&](Tx& t) {
        auto rows = t.query(
            "SELECT tp.id AS topic_id, tp.code, tp.name, coalesce(tm.mastery, 0.3) AS mastery, "
            "coalesce(tm.confidence, 0.1) AS confidence, coalesce(tm.attempts, 0) AS attempts, "
            "coalesce(tm.correct, 0) AS correct, tm.last_seen_at "
            "FROM topics tp LEFT JOIN topic_mastery tm ON tm.topic_id = tp.id AND tm.student_id = $2 "
            "WHERE tp.course_id = $1 ORDER BY tp.code",
            {param(courseId), param(ctx.userId)});
        json out = json::array();
        for (const auto& r : rows) {
            json item = r;
            item["name"] = I18n::pick(r["name"], ctx.locale);
            out.push_back(item);
        }
        return json{{"topics", out}};
    });
}

json AiService::tipsForTeacher(Tx& t, const std::string& studentId) {
    auto weak = t.one(
        "SELECT tp.name, tm.mastery FROM topic_mastery tm JOIN topics tp ON tp.id = tm.topic_id "
        "WHERE tm.student_id = $1 ORDER BY tm.mastery ASC LIMIT 1",
        {param(studentId)});
    return json{{"weak_topic", weak.is_null() ? "" : weak.value("name", std::string())},
                {"mastery", weak.is_null() ? 0.3 : asDouble(weak["mastery"], 0.3)}};
}

} // namespace lumina

