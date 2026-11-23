#include "services/course_service.h"
#include "core/i18n.h"
#include "core/util.h"
#include "services/grading.h"
#include "services/xp_engine.h"
#include <algorithm>
#include <set>

namespace lumina {

namespace {

json localizeCourse(const json& row, const std::string& locale) {
    json out = row;
    out["title"] = I18n::pick(row["title"], locale);
    out["description"] = I18n::pick(row["description"], locale);
    return out;
}

json localizeLevel(const json& row, const std::string& locale) {
    json out = row;
    out["title"] = I18n::pick(row["title"], locale);
    out["description"] = I18n::pick(row["description"], locale);
    return out;
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

json levelProgressRow(Tx& t, const std::string& studentId, const std::string& levelId) {
    return t.one("SELECT status, stars, best_accuracy, xp_earned, attempts_count, completed_at "
                 "FROM level_progress WHERE student_id = $1 AND level_id = $2",
                 {param(studentId), param(levelId)});
}

} // namespace

bool CourseService::levelUnlocked(Tx& t, const std::string& studentId, const json& level,
                                  const json& previousLevel) {
    if (asInt(level["order_no"]) <= 1) return true;
    if (!previousLevel.is_null()) {
        auto prevProgress =
            levelProgressRow(t, studentId, previousLevel["id"].get<std::string>());
        if (!prevProgress.is_null() && prevProgress.value("status", "") == "completed") return true;
    }
    const json rules = level.value("unlock_rules", json::object());
    const int minXp = asInt(rules.value("min_xp", json(0)));
    if (minXp <= 0) {
        // No XP gate: unlocking is driven by the previous level only.
        return false;
    }
    const long long totalXp =
        t.scalarInt("SELECT total_xp FROM students WHERE user_id = $1", {param(studentId)});
    if (totalXp < minXp) return false;
    if (previousLevel.is_null()) return true;
    auto prevProgress = levelProgressRow(t, studentId, previousLevel["id"].get<std::string>());
    const double minAcc = asDouble(rules.value("min_prev_accuracy", json(0.0)));
    const double prevAcc =
        prevProgress.is_null() ? 0.0 : asDouble(prevProgress.value("best_accuracy", json(0.0)));
    return prevAcc >= minAcc;
}

json CourseService::listCourses(const AuthCtx& ctx, const std::string& category,
                                const std::string& search, bool enrolledOnly) {
    return Db::instance().tx([&](Tx& t) {
        const bool isStudent = ctx.role == "student";
        std::string sql =
            "SELECT c.*, "
            "(SELECT count(*) FROM levels l WHERE l.course_id = c.id) AS levels_count, "
            "(SELECT count(*) FROM tasks tk JOIN levels l ON l.id = tk.level_id "
            " WHERE l.course_id = c.id) AS tasks_count, "
            "(SELECT count(*) FROM levels l WHERE l.course_id = c.id AND l.is_boss) AS boss_count, ";
        if (isStudent) {
            sql += "e.status AS enrollment_status, coalesce(e.progress_pct, 0) AS progress_pct, "
                   "COALESCE((SELECT count(*) FROM level_progress lp "
                   " JOIN levels l2 ON l2.id = lp.level_id "
                   " WHERE l2.course_id = c.id AND lp.student_id = "
                   "$1 AND lp.status = 'completed'), 0) AS completed_levels ";
        } else {
            sql += "NULL AS enrollment_status, 0 AS progress_pct, 0 AS completed_levels ";
        }
        sql += "FROM courses c ";
        if (isStudent) {
            sql += "LEFT JOIN course_enrollments e ON e.course_id = c.id AND e.student_id = $1 ";
        }
        sql += "WHERE c.status = 'published' ";
        Params params;
        if (isStudent) params.push_back(param(ctx.userId));
        if (!category.empty()) {
            params.push_back(param(category));
            sql += "AND c.category = $" + std::to_string(params.size()) + " ";
        }
        if (!search.empty()) {
            params.push_back(param("%" + toLower(search) + "%"));
            sql += "AND (lower(c.title->>'kk') LIKE $" + std::to_string(params.size()) +
                   " OR lower(c.title->>'ru') LIKE $" + std::to_string(params.size()) +
                   " OR lower(c.slug) LIKE $" + std::to_string(params.size()) + ") ";
        }
        if (enrolledOnly) sql += "AND e.student_id IS NOT NULL ";
        sql += "ORDER BY c.sort_order, c.created_at";

        json out = json::array();
        for (const auto& row : t.query(sql, params)) {
            json c = localizeCourse(row, ctx.locale);
            c["enrolled"] = !row["enrollment_status"].is_null();
            c["completed_levels"] = row.value("completed_levels", 0);
            out.push_back(c);
        }
        return out;
    });
}

json CourseService::getCourse(const AuthCtx& ctx, const std::string& courseId) {
    return Db::instance().tx([&](Tx& t) {
        auto course = t.one(
            "SELECT c.*, "
            "(SELECT count(*) FROM levels l WHERE l.course_id = c.id) AS levels_count, "
            "(SELECT count(*) FROM tasks tk JOIN levels l ON l.id = tk.level_id "
            " WHERE l.course_id = c.id) AS tasks_count, "
            "(SELECT count(*) FROM course_enrollments e WHERE e.course_id = c.id) AS students_count "
            "FROM courses c WHERE c.id = $1",
            {param(courseId)});
        if (course.is_null()) throw notFound("Course not found");

        json out = localizeCourse(course, ctx.locale);
        auto levels = t.query("SELECT * FROM levels WHERE course_id = $1 ORDER BY order_no",
                              {param(courseId)});
        json levelArr = json::array();
        json prev = nullptr;
        for (auto& lvl : levels) {
            json item = localizeLevel(lvl, ctx.locale);
            if (ctx.role == "student") {
                auto prog = levelProgressRow(t, ctx.userId, lvl["id"].get<std::string>());
                const bool completed = !prog.is_null() && prog.value("status", "") == "completed";
                const bool unlocked = completed || levelUnlocked(t, ctx.userId, lvl, prev);
                item["state"] = completed ? "completed" : (unlocked ? "available" : "locked");
                item["stars"] = prog.is_null() ? 0 : prog.value("stars", 0);
                item["best_accuracy"] = prog.is_null() ? 0.0 : asDouble(prog.value("best_accuracy", json(0.0)));
                item["xp_earned"] = prog.is_null() ? 0 : prog.value("xp_earned", 0);
            } else {
                item["state"] = "available";
                item["stars"] = 0;
            }
            item["tasks_count"] = t.scalarInt("SELECT count(*) FROM tasks WHERE level_id = $1",
                                              {param(lvl["id"].get<std::string>())});
            levelArr.push_back(item);
            prev = lvl;
        }
        out["levels"] = levelArr;
        if (ctx.role == "student") {
            auto enr = t.one("SELECT status, progress_pct FROM course_enrollments "
                             "WHERE student_id = $1 AND course_id = $2",
                             {param(ctx.userId), param(courseId)});
            out["enrolled"] = !enr.is_null();
            out["enrollment"] = enr;
        }
        return out;
    });
}

json CourseService::courseMap(const AuthCtx& ctx, const std::string& courseId) {
    return Db::instance().tx([&](Tx& t) {
        auto course = t.one("SELECT * FROM courses WHERE id = $1", {param(courseId)});
        if (course.is_null()) throw notFound("Course not found");

        auto levels = t.query(
            "SELECT l.*, "
            "(SELECT count(*) FROM tasks tk WHERE tk.level_id = l.id) AS tasks_count, "
            "(SELECT count(DISTINCT a.task_id) FROM attempts a JOIN tasks tk ON tk.id = a.task_id "
            " WHERE tk.level_id = l.id AND a.student_id = $2 AND a.result = 'correct') AS solved_tasks "
            "FROM levels l WHERE l.course_id = $1 ORDER BY l.order_no",
            {param(courseId), param(ctx.userId)});

        json arr = json::array();
        json prev = nullptr;
        json recommended = nullptr;
        for (auto& lvl : levels) {
            json item = localizeLevel(lvl, ctx.locale);
            auto prog = levelProgressRow(t, ctx.userId, lvl["id"].get<std::string>());
            const bool completed = !prog.is_null() && prog.value("status", "") == "completed";
            const bool unlocked = completed || levelUnlocked(t, ctx.userId, lvl, prev);
            item["state"] = completed ? "completed" : (unlocked ? "available" : "locked");
            item["stars"] = prog.is_null() ? 0 : prog.value("stars", 0);
            item["best_accuracy"] = prog.is_null() ? 0.0 : asDouble(prog.value("best_accuracy", json(0.0)));
            item["xp_earned"] = prog.is_null() ? 0 : prog.value("xp_earned", 0);
            // `recommended` starts as JSON null.  Using its boolean conversion
            // throws before the first available level can be selected.
            if (recommended.is_null() && !completed && unlocked) recommended = item;
            arr.push_back(item);
            prev = lvl;
        }
        return json{{"course", localizeCourse(course, ctx.locale)},
                    {"levels", arr},
                    {"recommended_level", recommended}};
    });
}

json CourseService::levelDetail(const AuthCtx& ctx, const std::string& levelId) {
    return Db::instance().tx([&](Tx& t) {
        auto level = t.one("SELECT * FROM levels WHERE id = $1", {param(levelId)});
        if (level.is_null()) throw notFound("Level not found");
        auto course = t.one("SELECT * FROM courses WHERE id = $1",
                            {param(level["course_id"].get<std::string>())});

        auto tasks = t.query(
            "SELECT tk.id, tk.type, tk.difficulty, tk.base_xp, tk.time_limit_sec, tk.sort_order, "
            "tk.topic_id, "
            "COALESCE((SELECT max(a.score) FROM attempts a WHERE a.task_id = tk.id "
            " AND a.student_id = $2), -1) AS best_score, "
            "COALESCE((SELECT count(*) FROM attempts a WHERE a.task_id = tk.id "
            " AND a.student_id = $2), 0) AS attempt_count "
            "FROM tasks tk WHERE tk.level_id = $1 AND tk.status = 'published' "
            "ORDER BY tk.sort_order, tk.created_at",
            {param(levelId), param(ctx.userId)});

        json taskArr = json::array();
        for (const auto& task : tasks) {
            json item = {{"id", task["id"]},
                         {"type", task["type"]},
                         {"difficulty", task["difficulty"]},
                         {"base_xp", task["base_xp"]},
                         {"time_limit_sec", task["time_limit_sec"]},
                         {"topic_id", task["topic_id"]},
                         {"attempts", task["attempt_count"]},
                         {"solved", asDouble(task["best_score"], -1) >= 0.999}};
            item["best_score"] = std::max(0.0, asDouble(task["best_score"], 0.0));
            taskArr.push_back(item);
        }

        auto prog = levelProgressRow(t, ctx.userId, levelId);
        auto prev = t.one("SELECT * FROM levels WHERE course_id = $1 AND order_no = $2",
                          {param(level["course_id"].get<std::string>()),
                           param(asInt(level["order_no"]) - 1)});
        const bool completed = !prog.is_null() && prog.value("status", "") == "completed";
        const bool unlocked = completed || levelUnlocked(t, ctx.userId, level, prev);

        json out = {{"level", localizeLevel(level, ctx.locale)},
                    {"course", localizeCourse(course, ctx.locale)},
                    {"tasks", taskArr},
                    {"state", completed ? "completed" : (unlocked ? "available" : "locked")},
                    {"progress", prog.is_null() ? json::object() : prog}};
        return out;
    });
}

json CourseService::enroll(const AuthCtx& ctx, const std::string& courseId) {
    return Db::instance().tx([&](Tx& t) {
        auto course = t.one("SELECT id, title, prerequisite_course_id, status FROM courses WHERE id = $1",
                            {param(courseId)});
        if (course.is_null()) throw notFound("Course not found");
        if (course["status"] != "published") throw badRequest("Course is not published");

        if (!course["prerequisite_course_id"].is_null()) {
            auto done = t.scalar(
                "SELECT 1 FROM course_enrollments e WHERE e.student_id = $1 AND e.course_id = $2 "
                "AND e.status = 'completed'",
                {param(ctx.userId), param(course["prerequisite_course_id"].get<std::string>())});
            if (!done) throw forbidden("Complete the prerequisite course first");
        }
        t.exec("INSERT INTO course_enrollments (student_id, course_id) VALUES ($1, $2) "
               "ON CONFLICT (student_id, course_id) DO UPDATE SET status = 'active'",
               {param(ctx.userId), param(courseId)});
        auto firstLevel = t.one("SELECT id FROM levels WHERE course_id = $1 ORDER BY order_no LIMIT 1",
                                {param(courseId)});
        return json{{"enrolled", true},
                    {"course_id", courseId},
                    {"first_level_id", firstLevel.is_null() ? json(nullptr) : firstLevel["id"]}};
    });
}

json CourseService::unenroll(const AuthCtx& ctx, const std::string& courseId) {
    return Db::instance().tx([&](Tx& t) {
        t.exec("UPDATE course_enrollments SET status = 'left' "
               "WHERE student_id = $1 AND course_id = $2",
               {param(ctx.userId), param(courseId)});
        return json{{"left", true}};
    });
}

json CourseService::nextTask(const AuthCtx& ctx, const std::string& levelId,
                             const std::string& excludeTaskId) {
    return Db::instance().tx([&](Tx& t) {
        auto level = t.one("SELECT * FROM levels WHERE id = $1", {param(levelId)});
        if (level.is_null()) throw notFound("Level not found");

        std::string sql =
            "SELECT tk.* FROM tasks tk WHERE tk.level_id = $1 AND tk.status = 'published' "
            "AND NOT EXISTS (SELECT 1 FROM attempts a WHERE a.task_id = tk.id "
            " AND a.student_id = $2 AND a.result = 'correct') ";
        Params params{param(levelId), param(ctx.userId)};
        if (!excludeTaskId.empty()) {
            sql += "AND tk.id <> $3 ";
            params.push_back(param(excludeTaskId));
        }
        sql += "ORDER BY tk.sort_order, tk.difficulty LIMIT 1";
        auto task = t.one(sql, params);
        if (task.is_null()) {
            // All tasks solved: offer the weakest one for repetition.
            task = t.one(
                "SELECT tk.* FROM tasks tk LEFT JOIN (SELECT task_id, max(score) AS best, "
                "max(created_at) AS last FROM attempts WHERE student_id = $2 GROUP BY task_id) a "
                "ON a.task_id = tk.id WHERE tk.level_id = $1 AND tk.status = 'published' "
                "ORDER BY coalesce(a.best, 0) ASC, a.last ASC NULLS FIRST LIMIT 1",
                {param(levelId), param(ctx.userId)});
        }
        if (task.is_null()) throw notFound("No tasks in this level");
        return Grading::sanitizeTask(task, ctx.locale);
    });
}

json CourseService::continueLearning(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        auto row = t.one(
            "SELECT c.id AS course_id, l.id AS level_id, l.title AS level_title, "
            "l.order_no, l.is_boss, c.title AS course_title, e.progress_pct "
            "FROM course_enrollments e JOIN courses c ON c.id = e.course_id "
            "JOIN levels l ON l.course_id = c.id "
            "LEFT JOIN level_progress lp ON lp.level_id = l.id AND lp.student_id = e.student_id "
            "WHERE e.student_id = $1 AND e.status = 'active' "
            "AND coalesce(lp.status, '') <> 'completed' "
            "ORDER BY e.updated_at DESC, l.order_no LIMIT 1",
            {param(ctx.userId)});
        if (row.is_null()) return json(nullptr);
        return json{{"course_id", row["course_id"]},
                    {"level_id", row["level_id"]},
                    {"level_title", I18n::pick(row["level_title"], ctx.locale)},
                    {"course_title", I18n::pick(row["course_title"], ctx.locale)},
                    {"order_no", row["order_no"]},
                    {"is_boss", row["is_boss"]},
                    {"progress_pct", row.value("progress_pct", 0.0)}};
    });
}

json CourseService::recomputeLevelProgress(Tx& t, const std::string& studentId,
                                           const std::string& levelId, const std::string& locale) {
    auto level = t.one("SELECT * FROM levels WHERE id = $1", {param(levelId)});
    if (level.is_null()) throw notFound("Level not found");

    auto stats = t.one(
        "SELECT count(*) AS tasks_total, "
        "count(*) FILTER (WHERE best IS NOT NULL) AS tasks_touched, "
        "coalesce(avg(best), 0) AS avg_best, "
        "count(*) FILTER (WHERE best >= 0.999) AS tasks_solved "
        "FROM (SELECT tk.id, (SELECT max(a.score) FROM attempts a "
        "  WHERE a.task_id = tk.id AND a.student_id = $2) AS best "
        "  FROM tasks tk WHERE tk.level_id = $1 AND tk.status = 'published') s",
        {param(levelId), param(studentId)});

    const int total = stats.value("tasks_total", 0);
    const int touched = stats.value("tasks_touched", 0);
    const double avg = asDouble(stats.value("avg_best", json(0.0)));
    const int solved = stats.value("tasks_solved", 0);
    const bool allTouched = total > 0 && touched >= total;
    const bool completed = allTouched && avg >= 0.6;

    int stars = 0;
    if (completed) {
        stars = avg >= 0.95 ? 3 : (avg >= 0.8 ? 2 : 1);
    }

    auto existing = t.one(
        "SELECT status, stars, xp_earned, completed_at FROM level_progress "
        "WHERE student_id = $1 AND level_id = $2",
        {param(studentId), param(levelId)});
    const bool wasCompleted = !existing.is_null() && existing.value("status", "") == "completed";
    const int oldStars = existing.is_null() ? 0 : existing.value("stars", 0);

    int bonusXp = 0;
    if (completed && (!wasCompleted || stars > oldStars)) {
        const int base = level.value("xp_reward", 100);
        bonusXp = level.value("is_boss", false) ? static_cast<int>(base * 1.5) : base;
        if (wasCompleted) bonusXp = bonusXp / 3;  // replay: reduced reward
    }

    t.exec(
        "INSERT INTO level_progress (student_id, level_id, status, stars, best_accuracy, "
        "xp_earned, attempts_count, completed_at) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8) "
        "ON CONFLICT (student_id, level_id) DO UPDATE SET status = EXCLUDED.status, "
        "stars = greatest(level_progress.stars, EXCLUDED.stars), "
        "best_accuracy = greatest(level_progress.best_accuracy, EXCLUDED.best_accuracy), "
        "xp_earned = level_progress.xp_earned + EXCLUDED.xp_earned, "
        "attempts_count = level_progress.attempts_count + 1, "
        "completed_at = coalesce(level_progress.completed_at, EXCLUDED.completed_at), "
        "updated_at = now()",
        {param(studentId), param(levelId), param(completed ? "completed" : "in_progress"),
         param(stars), param(avg), param(bonusXp), param(1),
         completed ? param(isoNow()) : std::optional<std::string>()});

    json out = {{"completed", completed},
                {"stars", stars},
                {"accuracy", avg},
                {"tasks_solved", solved},
                {"tasks_total", total},
                {"bonus_xp", bonusXp},
                {"first_completion", completed && !wasCompleted},
                {"level_title", I18n::pick(level["title"], locale)},
                {"is_boss", level.value("is_boss", false)},
                {"level_id", levelId},
                {"course_id", level["course_id"]}};
    if (completed) {
        t.exec("UPDATE course_enrollments SET last_level_id = $3, "
               "progress_pct = (SELECT round(100.0 * count(*) FILTER (WHERE lp.status = 'completed') "
               " / greatest(count(*), 1), 2) FROM levels l "
               " LEFT JOIN level_progress lp ON lp.level_id = l.id AND lp.student_id = $1 "
               " WHERE l.course_id = $2), updated_at = now() "
               "WHERE student_id = $1 AND course_id = $2",
               {param(studentId), param(level["course_id"].get<std::string>()), param(levelId)});
    }
    return out;
}

} // namespace lumina

