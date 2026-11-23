#include "http/controllers/system_controller.h"
#include "core/config.h"
#include "core/log.h"
#include "db/db.h"
#include "ws/ws_hub.h"
#include <chrono>
#include <fstream>
#include <sstream>

namespace lumina {

namespace {
const auto kStarted = std::chrono::steady_clock::now();

drogon::HttpResponsePtr text(const std::string& body, const std::string& contentType) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setContentTypeString(contentType);
    resp->setBody(body);
    return resp;
}
} // namespace

void SystemController::health(const HttpRequestPtr&, Callback&& cb) {
    try {
        Db::instance().query("SELECT 1 AS ok");
        const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::steady_clock::now() - kStarted)
                                .count();
        cb(http::ok({{"status", "ok"},
                     {"db", "ok"},
                     {"version", LUMINA_VERSION},
                     {"uptime_sec", uptime},
                     {"ws_online", Hub::instance().online()}}));
    } catch (const std::exception& e) {
        cb(http::fail(503, "unhealthy", e.what()));
    }
}

void SystemController::metrics(const HttpRequestPtr&, Callback&& cb) {
    try {
        const auto m = Db::instance().query(
            "SELECT (SELECT count(*) FROM users) AS users, "
            "(SELECT count(*) FROM students) AS students, "
            "(SELECT count(*) FROM attempts) AS attempts, "
            "(SELECT count(*) FROM xp_events) AS xp_events, "
            "(SELECT count(*) FROM courses WHERE status = 'published') AS courses, "
            "(SELECT count(*) FROM tasks WHERE status = 'published') AS tasks, "
            "(SELECT count(*) FROM anticheat_flags WHERE status = 'open') AS open_flags")[0];
        std::ostringstream out;
        out << "# HELP lumina_users_total Registered users\n# TYPE lumina_users_total gauge\n"
            << "lumina_users_total " << m.value("users", 0) << "\n"
            << "# HELP lumina_students_total Students\n# TYPE lumina_students_total gauge\n"
            << "lumina_students_total " << m.value("students", 0) << "\n"
            << "# HELP lumina_attempts_total Attempts\n# TYPE lumina_attempts_total counter\n"
            << "lumina_attempts_total " << m.value("attempts", 0) << "\n"
            << "# HELP lumina_xp_events_total XP events\n# TYPE lumina_xp_events_total counter\n"
            << "lumina_xp_events_total " << m.value("xp_events", 0) << "\n"
            << "# HELP lumina_courses_total Published courses\n# TYPE lumina_courses_total gauge\n"
            << "lumina_courses_total " << m.value("courses", 0) << "\n"
            << "# HELP lumina_tasks_total Published tasks\n# TYPE lumina_tasks_total gauge\n"
            << "lumina_tasks_total " << m.value("tasks", 0) << "\n"
            << "# HELP lumina_websocket_connections Open WebSocket connections\n"
            << "# TYPE lumina_websocket_connections gauge\n"
            << "lumina_websocket_connections " << Hub::instance().online() << "\n"
            << "# HELP lumina_anticheat_open Open anticheat flags\n"
            << "# TYPE lumina_anticheat_open gauge\n"
            << "lumina_anticheat_open " << m.value("open_flags", 0) << "\n";
        cb(text(out.str(), "text/plain; version=0.0.4; charset=utf-8"));
    } catch (const std::exception& e) {
        cb(http::fail(500, "metrics_error", e.what()));
    }
}

void SystemController::meta(const HttpRequestPtr&, Callback&& cb) {
    try {
        auto stats = Db::instance().query(
            "SELECT (SELECT count(*) FROM courses WHERE status='published') AS courses, "
            "(SELECT count(*) FROM tasks WHERE status='published') AS tasks, "
            "(SELECT count(*) FROM achievements) AS achievements, "
            "(SELECT count(*) FROM students) AS students, "
            "(SELECT count(*) FROM seasons WHERE status='active') AS active_seasons")[0];
        cb(http::ok({{"name", "Lumina"},
                     {"version", LUMINA_VERSION},
                     {"tagline", {{"kk", "РћТ›Сѓ вЂ” РѕР№С‹РЅ"}, {"ru", "РЈС‡С‘Р±Р° вЂ” РёРіСЂР°"}, {"en", "Learning is a game"}}},
                     {"locale_support", {"kk", "ru", "en"}},
                     {"stats", stats}}));
    } catch (const std::exception& e) {
        cb(http::fail(500, "meta_error", e.what()));
    }
}

void SystemController::openapi(const HttpRequestPtr&, Callback&& cb) {
    std::ifstream in("docs/openapi.yaml");
    if (!in) in.open("../docs/openapi.yaml");
    if (!in) {
        cb(http::fail(404, "not_found", "openapi.yaml not found"));
        return;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    cb(text(ss.str(), "application/yaml; charset=utf-8"));
}

void SystemController::docs(const HttpRequestPtr&, Callback&& cb) {
    static const char* html = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"/>
<title>Lumina API</title>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<link rel="stylesheet" href="https://unpkg.com/swagger-ui-dist@5/swagger-ui.css"/>
<style>body{margin:0;background:#f8f9ff;font-family:'Plus Jakarta Sans',system-ui,sans-serif}
.topbar{display:none}</style></head>
<body><div id="swagger-ui"></div>
<script src="https://unpkg.com/swagger-ui-dist@5/swagger-ui-bundle.js"></script>
<script>window.onload=()=>SwaggerUIBundle({url:'/openapi.yaml',dom_id:'#swagger-ui',deepLinking:true});</script>
</body></html>)HTML";
    cb(text(html, "text/html; charset=utf-8"));
}

} // namespace lumina

extern "C" void lumina_link_system_controller() {}


