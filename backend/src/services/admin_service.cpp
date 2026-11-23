#include "services/admin_service.h"
#include "core/config.h"
#include "core/i18n.h"
#include "core/log.h"
#include "core/util.h"
#include "services/leaderboard_service.h"
#include "services/season_service.h"
#include "services/xp_engine.h"

#include <algorithm>
#include <regex>
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

void requireText(const json& v, const std::string& field) {
    if (!v.is_object() || !v.contains("kk") || !v.contains("ru") ||
        !v["kk"].is_string() || !v["ru"].is_string()) {
        throw unprocessable("Field '" + field + "' must contain kk and ru strings");
    }
}

const std::set<std::string> kTaskTypes = {
    "single_choice", "multiple_choice", "true_false", "short_answer", "matching",
    "ordering", "cloze", "numeric", "timed_quiz", "code"};

std::string slugify(const std::string& s) {
    std::string out;
    for (char c : toLower(s)) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out += c;
        else if (c == ' ' || c == '-' || c == '_') out += '-';
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "course" : out;
}

} // namespace

void TeacherService::assertTeacherOfCourse(Tx& t, const AuthCtx& ctx, const std::string& courseId) {
    if (ctx.role == "admin") return;
    auto owner = t.scalar("SELECT author_id FROM courses WHERE id = $1", {param(courseId)});
    if (!owner || *owner != ctx.userId) throw forbidden("You can only edit your own courses");
}

json TeacherService::courses(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        auto rows = t.query(
            "SELECT c.*, (SELECT count(*) FROM levels l WHERE l.course_id = c.id) AS levels, "
            "(SELECT count(*) FROM course_enrollments e WHERE e.course_id = c.id) AS students "
            "FROM courses c WHERE ($1 = 'admin' OR c.author_id = $2) ORDER BY c.created_at DESC",
            {param(ctx.role), param(ctx.userId)});
        json out = json::array();
        for (const auto& r : rows) out.push_back(localizeCourse(r, ctx.locale));
        return json{{"courses", out}};
    });
}

json TeacherService::createCourse(const AuthCtx& ctx, const json& body) {
    requireText(body.value("title", json::object()), "title");
    requireText(body.value("description", json::object()), "description");
    const std::string slug = slugify(body.value("slug", I18n::pick(body["title"], "en")) + "-" +
                                     randomHex(2));
    const std::string category = body.value("category", "general");
    const std::string cover = body.value("cover", "school");
    const int difficulty = std::clamp(body.value("difficulty", 3), 1, 10);

    return Db::instance().tx([&](Tx& t) {
        const std::string id = uuidV4();
        t.exec("INSERT INTO courses (id, slug, title, description, category, tags, cover, "
               "difficulty, status, author_id) VALUES ($1,$2,$3::jsonb,$4::jsonb,$5,$6::jsonb,$7,$8,'draft',$9)",
               {param(id), param(slug), paramJson(body["title"]), paramJson(body["description"]),
                param(category), paramJson(body.value("tags", json::array())), param(cover),
                param(difficulty), param(ctx.userId)});
        return json{{"id", id}, {"slug", slug}};
    });
}

json TeacherService::updateCourse(const AuthCtx& ctx, const std::string& id, const json& body) {
    return Db::instance().tx([&](Tx& t) {
        assertTeacherOfCourse(t, ctx, id);
        auto course = t.one("SELECT * FROM courses WHERE id = $1", {param(id)});
        if (course.is_null()) throw notFound("Course not found");
        json title = body.contains("title") ? body["title"] : course["title"];
        json desc = body.contains("description") ? body["description"] : course["description"];
        requireText(title, "title");
        requireText(desc, "description");
        t.exec("UPDATE courses SET title = $1::jsonb, description = $2::jsonb, "
               "category = $3, cover = $4, difficulty = $5, status = $6, tags = $7::jsonb, "
               "updated_at = now() WHERE id = $8",
               {paramJson(title), paramJson(desc), param(body.value("category", course["category"].get<std::string>())),
                param(body.value("cover", course["cover"].get<std::string>())),
                param(std::clamp(body.value("difficulty", course.value("difficulty", 3)), 1, 10)),
                param(body.value("status", course["status"].get<std::string>())),
                paramJson(body.value("tags", course["tags"])), param(id)});
        return json{{"updated", true}};
    });
}

json TeacherService::deleteCourse(const AuthCtx& ctx, const std::string& id) {
    return Db::instance().tx([&](Tx& t) {
        assertTeacherOfCourse(t, ctx, id);
        t.exec("UPDATE courses SET status = 'archived', updated_at = now() WHERE id = $1", {param(id)});
        return json{{"archived", true}};
    });
}

json TeacherService::topics(const AuthCtx& ctx, const std::string& courseId) {
    return Db::instance().tx([&](Tx& t) {
        auto rows = t.query("SELECT * FROM topics WHERE course_id = $1 ORDER BY code", {param(courseId)});
        json out = json::array();
        for (const auto& r : rows) {
            json item = r;
            item["name"] = I18n::pick(r["name"], ctx.locale);
            out.push_back(item);
        }
        return json{{"topics", out}};
    });
}

json TeacherService::createTopic(const AuthCtx& ctx, const json& body) {
    const std::string courseId = body.value("course_id", "");
    requireText(body.value("name", json::object()), "name");
    const std::string code = body.value("code", "");
    if (code.empty()) throw unprocessable("code is required");
    return Db::instance().tx([&](Tx& t) {
        assertTeacherOfCourse(t, ctx, courseId);
        const std::string id = uuidV4();
        t.exec("INSERT INTO topics (id, course_id, code, name) VALUES ($1,$2,$3,$4::jsonb)",
               {param(id), param(courseId), param(code), paramJson(body["name"])});
        return json{{"id", id}};
    });
}

json TeacherService::createLevel(const AuthCtx& ctx, const json& body) {
    const std::string courseId = body.value("course_id", "");
    requireText(body.value("title", json::object()), "title");
    requireText(body.value("description", json::object()), "description");
    return Db::instance().tx([&](Tx& t) {
        assertTeacherOfCourse(t, ctx, courseId);
        int order = body.value("order_no", 0);
        if (order <= 0) {
            order = static_cast<int>(t.scalarInt(
                        "SELECT coalesce(max(order_no), 0) + 1 FROM levels WHERE course_id = $1",
                        {param(courseId)}));
        }
        const std::string id = uuidV4();
        t.exec("INSERT INTO levels (id, course_id, order_no, title, description, icon, unlock_rules, "
               "is_boss, xp_reward) VALUES ($1,$2,$3,$4::jsonb,$5::jsonb,$6,$7::jsonb,$8,$9)",
               {param(id), param(courseId), param(order), paramJson(body["title"]),
                paramJson(body["description"]), param(body.value("icon", "flag")),
                paramJson(body.value("unlock_rules", json::object())),
                param(body.value("is_boss", false)), param(body.value("xp_reward", 100))});
        return json{{"id", id}, {"order_no", order}};
    });
}

json TeacherService::updateLevel(const AuthCtx& ctx, const std::string& id, const json& body) {
    return Db::instance().tx([&](Tx& t) {
        auto level = t.one("SELECT * FROM levels WHERE id = $1", {param(id)});
        if (level.is_null()) throw notFound("Level not found");
        assertTeacherOfCourse(t, ctx, level["course_id"].get<std::string>());
        json title = body.contains("title") ? body["title"] : level["title"];
        json desc = body.contains("description") ? body["description"] : level["description"];
        requireText(title, "title");
        requireText(desc, "description");
        t.exec("UPDATE levels SET title=$1::jsonb, description=$2::jsonb, icon=$3, "
               "is_boss=$4, xp_reward=$5, unlock_rules=$6::jsonb, order_no=$7 WHERE id=$8",
               {paramJson(title), paramJson(desc), param(body.value("icon", level["icon"].get<std::string>())),
                param(body.value("is_boss", level.value("is_boss", false))),
                param(body.value("xp_reward", level.value("xp_reward", 100))),
                paramJson(body.value("unlock_rules", level["unlock_rules"])),
                param(body.value("order_no", level.value("order_no", 1))), param(id)});
        return json{{"updated", true}};
    });
}

json TeacherService::deleteLevel(const AuthCtx& ctx, const std::string& id) {
    return Db::instance().tx([&](Tx& t) {
        auto level = t.one("SELECT course_id FROM levels WHERE id = $1", {param(id)});
        if (level.is_null()) throw notFound("Level not found");
        assertTeacherOfCourse(t, ctx, level["course_id"].get<std::string>());
        t.exec("DELETE FROM levels WHERE id = $1", {param(id)});
        return json{{"deleted", true}};
    });
}

json TeacherService::createTask(const AuthCtx& ctx, const json& body) {
    return Db::instance().tx([&](Tx& t) {
        const std::string levelId = body.value("level_id", "");
        auto level = t.one("SELECT * FROM levels WHERE id = $1", {param(levelId)});
        if (level.is_null()) throw notFound("Level not found");
        assertTeacherOfCourse(t, ctx, level["course_id"].get<std::string>());

        const std::string type = body.value("type", "");
        if (!kTaskTypes.count(type)) throw unprocessable("Unknown task type");
        if (!body.contains("answer_key") || !body["answer_key"].is_object()) {
            throw unprocessable("answer_key is required");
        }
        json prompt = body.value("prompt", json::object());
        requireText(prompt, "prompt");

        const std::string topicId = body.value("topic_id", "");
        auto topic = t.one("SELECT id FROM topics WHERE id = $1 AND course_id = $2",
                           {param(topicId), param(level["course_id"].get<std::string>())});
        if (topic.is_null()) throw unprocessable("topic_id does not belong to the course");

        const std::string id = uuidV4();
        int sort = body.value("sort_order", 0);
        if (sort <= 0) {
            sort = static_cast<int>(t.scalarInt(
                "SELECT coalesce(max(sort_order), 0) + 1 FROM tasks WHERE level_id = $1",
                {param(levelId)}));
        }
        t.exec("INSERT INTO tasks (id, level_id, topic_id, type, prompt, payload, answer_key, "
               "explanation, hints, difficulty, base_xp, time_limit_sec, status, sort_order) "
               "VALUES ($1,$2,$3,$4,$5::jsonb,$6::jsonb,$7::jsonb,$8::jsonb,$9::jsonb,$10,$11,$12,$13,$14)",
               {param(id), param(levelId), param(topicId), param(type), paramJson(prompt),
                paramJson(body.value("payload", json::object())), paramJson(body["answer_key"]),
                paramJson(body.value("explanation", json::object())),
                paramJson(body.value("hints", json::array())),
                param(std::clamp(body.value("difficulty", 3), 1, 10)),
                param(std::clamp(body.value("base_xp", 30), 5, 500)),
                param(std::clamp(body.value("time_limit_sec", 120), 10, 3600)),
                param(body.value("status", "published")), param(sort)});
        return json{{"id", id}, {"sort_order", sort}};
    });
}

json TeacherService::updateTask(const AuthCtx& ctx, const std::string& id, const json& body) {
    return Db::instance().tx([&](Tx& t) {
        auto task = t.one("SELECT tk.*, l.course_id FROM tasks tk JOIN levels l ON l.id = tk.level_id "
                          "WHERE tk.id = $1",
                          {param(id)});
        if (task.is_null()) throw notFound("Task not found");
        assertTeacherOfCourse(t, ctx, task["course_id"].get<std::string>());
        json prompt = body.contains("prompt") ? body["prompt"] : task["prompt"];
        requireText(prompt, "prompt");
        t.exec("UPDATE tasks SET prompt=$1::jsonb, payload=$2::jsonb, answer_key=$3::jsonb, "
               "explanation=$4::jsonb, hints=$5::jsonb, difficulty=$6, base_xp=$7, "
               "time_limit_sec=$8, status=$9, version=version+1, updated_at=now() WHERE id=$10",
               {paramJson(prompt), paramJson(body.value("payload", task["payload"])),
                paramJson(body.value("answer_key", task["answer_key"])),
                paramJson(body.value("explanation", task["explanation"])),
                paramJson(body.value("hints", task["hints"])),
                param(std::clamp(body.value("difficulty", task.value("difficulty", 3)), 1, 10)),
                param(std::clamp(body.value("base_xp", task.value("base_xp", 30)), 5, 500)),
                param(std::clamp(body.value("time_limit_sec", task.value("time_limit_sec", 120)), 10, 3600)),
                param(body.value("status", task.value("status", "published"))), param(id)});
        return json{{"updated", true}};
    });
}

json TeacherService::deleteTask(const AuthCtx& ctx, const std::string& id) {
    return Db::instance().tx([&](Tx& t) {
        auto task = t.one("SELECT l.course_id FROM tasks tk JOIN levels l ON l.id = tk.level_id "
                          "WHERE tk.id = $1",
                          {param(id)});
        if (task.is_null()) throw notFound("Task not found");
        assertTeacherOfCourse(t, ctx, task["course_id"].get<std::string>());
        t.exec("UPDATE tasks SET status = 'archived', updated_at = now() WHERE id = $1", {param(id)});
        return json{{"archived", true}};
    });
}

json TeacherService::importTasks(const AuthCtx& ctx, const json& body) {
    const json tasks = body.value("tasks", json::array());
    if (!tasks.is_array() || tasks.empty()) throw unprocessable("tasks[] is required");
    if (tasks.size() > 200) throw unprocessable("At most 200 tasks per import");
    json ids = json::array();
    for (const auto& task : tasks) {
        auto created = createTask(ctx, task);
        ids.push_back(created["id"]);
    }
    return json{{"imported", ids.size()}, {"ids", ids}};
}

json TeacherService::analytics(const AuthCtx& ctx, const std::string& courseId) {
    return Db::instance().tx([&](Tx& t) {
        auto course = t.one("SELECT * FROM courses WHERE id = $1", {param(courseId)});
        if (course.is_null()) throw notFound("Course not found");
        assertTeacherOfCourse(t, ctx, courseId);

        auto students = t.query(
            "SELECT s.user_id, s.nickname, s.avatar, s.level, s.total_xp, s.current_streak, "
            "e.progress_pct, e.updated_at AS enrolled_at, "
            "coalesce(st.attempts, 0) AS attempts, coalesce(st.accuracy, 0) AS accuracy, "
            "st.last_attempt_at "
            "FROM course_enrollments e JOIN students s ON s.user_id = e.student_id "
            "LEFT JOIN LATERAL (SELECT count(a.id) AS attempts, round(avg(a.score) * 100, 1) AS accuracy, "
            " max(a.created_at) AS last_attempt_at FROM attempts a JOIN tasks tk ON tk.id = a.task_id "
            " JOIN levels l ON l.id = tk.level_id WHERE l.course_id = $1 AND a.student_id = s.user_id) st "
            " ON true WHERE e.course_id = $1 AND e.status <> 'left' ORDER BY s.total_xp DESC",
            {param(courseId)});
        json studentArr = json::array();
        json risk = json::array();
        for (const auto& s : students) {
            json item = s;
            const int inactive = static_cast<int>(t.scalarInt(
                "SELECT coalesce(extract(day FROM now() - max(created_at))::int, 30) FROM attempts "
                "WHERE student_id = $1",
                {param(s["user_id"].get<std::string>())}));
            item["days_inactive"] = inactive;
            const bool atRisk = s.value("progress_pct", 0.0) < 25.0 || inactive >= 7 ||
                                (s.value("attempts", 0) >= 5 && s.value("accuracy", 0.0) < 50.0);
            item["at_risk"] = atRisk;
            if (atRisk) risk.push_back(item);
            studentArr.push_back(item);
        }

        auto weakTopics = t.query(
            "SELECT tp.name, count(a.id) AS attempts, round(avg(a.score) * 100, 1) AS accuracy, "
            "count(*) FILTER (WHERE a.result = 'wrong') AS errors "
            "FROM attempts a JOIN tasks tk ON tk.id = a.task_id JOIN topics tp ON tp.id = tk.topic_id "
            "JOIN levels l ON l.id = tk.level_id WHERE l.course_id = $1 "
            "GROUP BY tp.name HAVING count(a.id) > 0 ORDER BY accuracy ASC LIMIT 10",
            {param(courseId)});
        json weak = json::array();
        for (const auto& w : weakTopics) {
            json item = w;
            item["name"] = I18n::pick(w["name"], ctx.locale);
            weak.push_back(item);
        }

        auto distribution = t.query(
            "SELECT tk.difficulty, count(a.id) AS attempts, "
            "round(avg(CASE WHEN a.result = 'correct' THEN 1.0 ELSE 0.0 END) * 100, 1) AS success_rate "
            "FROM tasks tk LEFT JOIN attempts a ON a.task_id = tk.id "
            "JOIN levels l ON l.id = tk.level_id WHERE l.course_id = $1 "
            "GROUP BY tk.difficulty ORDER BY tk.difficulty",
            {param(courseId)});

        return json{{"course", localizeCourse(course, ctx.locale)},
                    {"students", studentArr},
                    {"risk", risk},
                    {"weak_topics", weak},
                    {"difficulty_distribution", distribution},
                    {"summary", {{"students", studentArr.size()},
                                 {"at_risk", risk.size()},
                                 {"avg_accuracy", studentArr.empty()
                                                        ? 0.0
                                                        : t.scalarDouble(
                                                              "SELECT round(avg(score) * 100, 1) FROM attempts a "
                                                              "JOIN tasks tk ON tk.id = a.task_id "
                                                              "JOIN levels l ON l.id = tk.level_id "
                                                              "WHERE l.course_id = $1",
                                                              {param(courseId)})}}}};
    });
}

// ---------------------------- Admin ----------------------------

json AdminService::users(const AuthCtx& ctx, const std::string& search, int limit, int offset) {
    limit = std::clamp(limit, 1, 200);
    offset = std::max(0, offset);
    return Db::instance().tx([&](Tx& t) {
        Params params{param(limit), param(offset)};
        std::string where;
        if (!search.empty()) {
            params.push_back(param("%" + toLower(search) + "%"));
            where = "WHERE lower(u.email) LIKE $3 OR lower(u.display_name) LIKE $3 ";
            params.clear();
            params = {param("%" + toLower(search) + "%"), param(limit), param(offset)};
            where = "WHERE lower(u.email) LIKE $1 OR lower(u.display_name) LIKE $1 ";
            auto rows = t.query(
                "SELECT u.id, u.email, u.role, u.locale, u.status, u.display_name, u.created_at, "
                "s.nickname, s.total_xp, s.level, s.coins FROM users u "
                "LEFT JOIN students s ON s.user_id = u.id " + where +
                "ORDER BY u.created_at DESC LIMIT $2 OFFSET $3",
                params);
            return json{{"users", rows}, {"limit", limit}, {"offset", offset}};
        }
        auto rows = t.query(
            "SELECT u.id, u.email, u.role, u.locale, u.status, u.display_name, u.created_at, "
            "s.nickname, s.total_xp, s.level, s.coins FROM users u "
            "LEFT JOIN students s ON s.user_id = u.id ORDER BY u.created_at DESC LIMIT $1 OFFSET $2",
            params);
        return json{{"users", rows}, {"limit", limit}, {"offset", offset}};
    });
}

json AdminService::updateUser(const AuthCtx& ctx, const std::string& id, const json& body) {
    return Db::instance().tx([&](Tx& t) {
        auto user = t.one("SELECT id, role, status FROM users WHERE id = $1", {param(id)});
        if (user.is_null()) throw notFound("User not found");
        if (body.contains("role")) {
            const std::string role = body["role"].get<std::string>();
            if (role != "student" && role != "teacher" && role != "admin") {
                throw unprocessable("Invalid role");
            }
            t.exec("UPDATE users SET role = $1, updated_at = now() WHERE id = $2", {param(role), param(id)});
        }
        if (body.contains("status")) {
            const std::string status = body["status"].get<std::string>();
            if (status != "active" && status != "blocked") throw unprocessable("Invalid status");
            t.exec("UPDATE users SET status = $1, updated_at = now() WHERE id = $2",
                   {param(status), param(id)});
            if (status == "blocked") {
                t.exec("UPDATE refresh_tokens SET revoked_at = now() WHERE user_id = $1 "
                       "AND revoked_at IS NULL",
                       {param(id)});
            }
        }
        t.exec("INSERT INTO audit_log (actor_id, action, entity_type, entity_id, payload) "
               "VALUES ($1, 'user_updated', 'user', $2, $3::jsonb)",
               {param(ctx.userId), param(id), paramJson(body)});
        return json{{"updated", true}};
    });
}

json AdminService::anticheat(const AuthCtx& ctx, const std::string& status, int limit) {
    (void)ctx;
    limit = std::clamp(limit, 1, 200);
    return Db::instance().tx([&](Tx& t) {
        Params params{param(limit)};
        std::string where;
        if (!status.empty()) {
            where = "WHERE f.status = $2";
            params.push_back(param(status));
        }
        auto rows = t.query(
            "SELECT f.*, s.nickname, t.prompt FROM anticheat_flags f "
            "JOIN students s ON s.user_id = f.student_id "
            "LEFT JOIN attempts a ON a.id = f.attempt_id LEFT JOIN tasks t ON t.id = a.task_id " +
                where + " ORDER BY f.created_at DESC LIMIT $1",
            params);
        json out = json::array();
        for (const auto& r : rows) {
            json item = r;
            item["prompt"] = I18n::pick(r["prompt"], ctx.locale);
            out.push_back(item);
        }
        return json{{"flags", out}};
    });
}

json AdminService::resolveFlag(const AuthCtx& ctx, const std::string& id, const json& body) {
    return Db::instance().tx([&](Tx& t) {
        const std::string status = body.value("status", "reviewed");
        if (status != "reviewed" && status != "dismissed") throw unprocessable("Invalid status");
        t.exec("UPDATE anticheat_flags SET status = $1 WHERE id = $2", {param(status), param(id)});
        t.exec("INSERT INTO audit_log (actor_id, action, entity_type, entity_id, payload) "
               "VALUES ($1, 'anticheat_reviewed', 'flag', $2, $3::jsonb)",
               {param(ctx.userId), param(id), paramJson(body)});
        return json{{"updated", true}};
    });
}

json AdminService::seasons(const AuthCtx& ctx) {
    return SeasonService::list(ctx);
}

json AdminService::createSeason(const AuthCtx& ctx, const json& body) {
    requireText(body.value("title", json::object()), "title");
    const std::string code = body.value("code", "season-" + randomHex(3));
    const std::string starts = body.value("starts_at", isoNow());
    const std::string ends = body.value("ends_at", isoNow());
    return Db::instance().tx([&](Tx& t) {
        const std::string id = uuidV4();
        t.exec("INSERT INTO seasons (id, code, title, theme, description, starts_at, ends_at, status, rewards) "
               "VALUES ($1,$2,$3::jsonb,$4,$5::jsonb,$6::timestamptz,$7::timestamptz,$8,$9::jsonb)",
               {param(id), param(code), paramJson(body["title"]), param(body.value("theme", "")),
                paramJson(body.value("description", json::object())), param(starts), param(ends),
                param(body.value("status", "upcoming")),
                paramJson(body.value("rewards", json::array()))});
        const json rewards = body.value("rewards_detail", json::array());
        for (const auto& r : rewards) {
            t.exec("INSERT INTO season_rewards (id, season_id, tier, title, description, required_xp, payload) "
                   "VALUES ($1,$2,$3,$4::jsonb,$5::jsonb,$6,$7::jsonb)",
                   {param(uuidV4()), param(id), param(r.value("tier", 1)),
                    paramJson(r.value("title", json::object())),
                    paramJson(r.value("description", json::object())),
                    param(r.value("required_xp", 0)), paramJson(r.value("payload", json::object()))});
        }
        return json{{"id", id}, {"code", code}};
    });
}

json AdminService::updateSeason(const AuthCtx& ctx, const std::string& id, const json& body) {
    return Db::instance().tx([&](Tx& t) {
        t.exec("UPDATE seasons SET title = coalesce($1::jsonb, title), theme = coalesce($2, theme), "
               "starts_at = coalesce($3::timestamptz, starts_at), "
               "ends_at = coalesce($4::timestamptz, ends_at), status = coalesce($5, status) WHERE id = $6",
               {body.contains("title") ? paramJson(body["title"]) : std::nullopt,
                body.contains("theme") ? param(body["theme"].get<std::string>()) : std::nullopt,
                body.contains("starts_at") ? param(body["starts_at"].get<std::string>()) : std::nullopt,
                body.contains("ends_at") ? param(body["ends_at"].get<std::string>()) : std::nullopt,
                body.contains("status") ? param(body["status"].get<std::string>()) : std::nullopt,
                param(id)});
        (void)ctx;
        return json{{"updated", true}};
    });
}

json AdminService::closeSeason(const AuthCtx& ctx, const std::string& id) {
    (void)ctx;
    SeasonService::closeSeason(id);
    return json{{"closed", true}};
}

json AdminService::createAchievement(const AuthCtx& ctx, const json& body) {
    requireText(body.value("title", json::object()), "title");
    requireText(body.value("description", json::object()), "description");
    if (!body.contains("rule") || !body["rule"].is_object()) throw unprocessable("rule is required");
    return Db::instance().tx([&](Tx& t) {
        const std::string id = uuidV4();
        t.exec("INSERT INTO achievements (id, code, category, rarity, rule, title, description, icon, "
               "points, reward_coins, is_secret, sort_order) "
               "VALUES ($1,$2,$3,$4,$5::jsonb,$6::jsonb,$7::jsonb,$8,$9,$10,$11,$12)",
               {param(id), param(body.value("code", "custom-" + randomHex(3))),
                param(body.value("category", "custom")), param(body.value("rarity", "common")),
                paramJson(body["rule"]), paramJson(body["title"]), paramJson(body["description"]),
                param(body.value("icon", "emoji_events")), param(body.value("points", 10)),
                param(body.value("reward_coins", 0)), param(body.value("is_secret", false)),
                param(body.value("sort_order", 999))});
        t.exec("INSERT INTO audit_log (actor_id, action, entity_type, entity_id, payload) "
               "VALUES ($1, 'achievement_created', 'achievement', $2, $3::jsonb)",
               {param(ctx.userId), param(id), paramJson(body)});
        return json{{"id", id}};
    });
}

json AdminService::metrics(const AuthCtx& ctx) {
    (void)ctx;
    return Db::instance().tx([&](Tx& t) {
        return json{
            {"users", t.scalarInt("SELECT count(*) FROM users WHERE deleted_at IS NULL")},
            {"students", t.scalarInt("SELECT count(*) FROM students")},
            {"teachers", t.scalarInt("SELECT count(*) FROM users WHERE role = 'teacher'")},
            {"courses", t.scalarInt("SELECT count(*) FROM courses WHERE status = 'published'")},
            {"tasks", t.scalarInt("SELECT count(*) FROM tasks WHERE status = 'published'")},
            {"attempts", t.scalarInt("SELECT count(*) FROM attempts")},
            {"attempts_today", t.scalarInt("SELECT count(*) FROM attempts WHERE created_at > now() - interval '1 day'")},
            {"xp_today", t.scalarInt("SELECT coalesce(sum(amount),0) FROM xp_events WHERE created_at > now() - interval '1 day'")},
            {"active_7d", t.scalarInt("SELECT count(*) FROM students WHERE last_active_at > now() - interval '7 days'")},
            {"open_flags", t.scalarInt("SELECT count(*) FROM anticheat_flags WHERE status = 'open'")},
            {"season", t.one("SELECT id, code, title, status FROM seasons WHERE status = 'active' LIMIT 1")},
        };
    });
}

json AdminService::audit(const AuthCtx& ctx, int limit) {
    (void)ctx;
    limit = std::clamp(limit, 1, 200);
    return Db::instance().tx([&](Tx& t) {
        return json{{"entries", t.query(
                                 "SELECT a.*, u.email FROM audit_log a LEFT JOIN users u ON u.id = a.actor_id "
                                 "ORDER BY a.created_at DESC LIMIT $1",
                                 {param(limit)})}};
    });
}

json AdminService::economy(const AuthCtx& ctx, const json& body) {
    (void)ctx;
    (void)body;
    return Db::instance().tx([&](Tx& t) {
        auto coins = t.one("SELECT coalesce(sum(price_coins),0) AS spent, count(*) AS purchases FROM purchases");
        auto xp = t.one("SELECT coalesce(sum(amount),0) AS total, coalesce(avg(amount),0) AS avg FROM xp_events "
                        "WHERE amount > 0");
        auto top = t.query("SELECT nickname, coins FROM students ORDER BY coins DESC LIMIT 5");
        return json{{"coins_spent", coins.value("spent", 0)},
                    {"purchases", coins.value("purchases", 0)},
                    {"xp_total", xp.value("total", 0)},
                    {"xp_avg", xp.value("avg", 0.0)},
                    {"richest", top},
                    {"formula", "base_xp Г— difficulty Г— accuracy Г— speed Г— streak Г— (1-hint) Г— repeat_decay"},
                    {"xp_to_next", "round(100 Г— level^1.5)"}};
    });
}

} // namespace lumina

