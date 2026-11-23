#include "services/report_service.h"
#include "core/i18n.h"
#include "core/log.h"
#include "core/util.h"
#include "services/achievement_service.h"
#include "services/leaderboard_service.h"
#include "services/xp_engine.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#if defined(LUMINA_HAVE_XLSX)
#include <xlsxwriter.h>
#endif
#if defined(LUMINA_HAVE_HPDF)
#include <hpdf.h>
#include <algorithm>
#endif

namespace lumina {

namespace fs = std::filesystem;

namespace {

json localizedRows(Tx& t, const std::string& sql, const Params& params,
                   const std::string& locale) {
    json out = json::array();
    for (const auto& r : t.query(sql, params)) {
        json item = r;
        if (r.contains("title")) item["title"] = I18n::pick(r["title"], locale);
        if (r.contains("name")) item["name"] = I18n::pick(r["name"], locale);
        out.push_back(item);
    }
    return out;
}

std::string csvEscape(const std::string& s) {
    if (s.find_first_of(";\",\n\r") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += "\"";
    return out;
}

struct Table {
    std::string title;
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
};

std::string cell(const json& v) {
    if (v.is_null()) return "";
    if (v.is_string()) return v.get<std::string>();
    if (v.is_number_float()) {
        std::ostringstream ss;
        ss << v.get<double>();
        return ss.str();
    }
    if (v.is_number_integer()) return std::to_string(v.get<long long>());
    if (v.is_boolean()) return v.get<bool>() ? "true" : "false";
    return v.dump();
}

Table toTable(const std::string& title, const json& rows, const std::vector<std::string>& cols) {
    Table t;
    t.title = title;
    for (const auto& c : cols) {
        if (c == "title" || c == "name") t.headers.push_back(title + "_" + c);
        else t.headers.push_back(c);
    }
    for (const auto& r : rows) {
        std::vector<std::string> line;
        for (const auto& c : cols) line.push_back(cell(r.value(c, json(""))));
        t.rows.push_back(line);
    }
    return t;
}

std::string tableCsv(const Table& t) {
    std::ostringstream out;
    out << "\xEF\xBB\xBF";  // UTF-8 BOM for Excel
    for (size_t i = 0; i < t.headers.size(); ++i) {
        if (i) out << ";";
        out << csvEscape(t.headers[i]);
    }
    out << "\r\n";
    for (const auto& row : t.rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i) out << ";";
            out << csvEscape(row[i]);
        }
        out << "\r\n";
    }
    return out.str();
}

#if defined(LUMINA_HAVE_XLSX)
std::string tableXlsx(const std::vector<Table>& tables) {
    const fs::path path = fs::temp_directory_path() / ("lumina_report_" + randomHex(6) + ".xlsx");
    lxw_workbook* wb = workbook_new(path.string().c_str());
    lxw_format* header = workbook_add_format(wb);
    format_set_bold(header);
    format_set_bg_color(header, 0xE5EEFF);
    for (size_t ti = 0; ti < tables.size(); ++ti) {
        const auto& t = tables[ti];
        lxw_worksheet* ws = workbook_add_worksheet(wb, ("Sheet" + std::to_string(ti + 1)).c_str());
        for (size_t c = 0; c < t.headers.size(); ++c) {
            worksheet_write_string(ws, 0, (lxw_col_t)c, t.headers[c].c_str(), header);
        }
        for (size_t r = 0; r < t.rows.size(); ++r) {
            for (size_t c = 0; c < t.rows[r].size(); ++c) {
                worksheet_write_string(ws, (lxw_row_t)(r + 1), (lxw_col_t)c,
                                       t.rows[r][c].c_str(), nullptr);
            }
        }
        worksheet_set_column(ws, 0, (lxw_col_t)(t.headers.empty() ? 0 : t.headers.size() - 1), 24, nullptr);
    }
    workbook_close(wb);
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    std::error_code ec;
    fs::remove(path, ec);
    return ss.str();
}
#endif

#if defined(LUMINA_HAVE_HPDF)
std::string tablePdf(const Table& t) {
    const fs::path path = fs::temp_directory_path() / ("lumina_report_" + randomHex(6) + ".pdf");
    HPDF_Doc pdf = HPDF_New(nullptr, nullptr);
    if (!pdf) return {};
    HPDF_UseUTFEncodings(pdf);

    const char* ttfCandidates[] = {
        "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"};
    HPDF_Font font = nullptr;
    for (const char* f : ttfCandidates) {
        if (!fs::exists(f)) continue;
        const char* fontName = HPDF_LoadTTFontFromFile(pdf, f, HPDF_TRUE);
        if (fontName) font = HPDF_GetFont(pdf, fontName, nullptr);
        if (font) break;
    }

    HPDF_Page page = HPDF_AddPage(pdf);
    HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_LANDSCAPE);
    if (font) HPDF_Page_SetFontAndSize(page, font, 9);
    HPDF_REAL y = HPDF_Page_GetHeight(page) - 40;
    if (font) {
        HPDF_Page_BeginText(page);
        HPDF_Page_SetFontAndSize(page, font, 14);
        HPDF_Page_TextOut(page, 30, y, t.title.c_str());
        HPDF_Page_EndText(page);
        y -= 24;
        HPDF_Page_BeginText(page);
        HPDF_Page_SetFontAndSize(page, font, 8);
        for (size_t c = 0; c < t.headers.size(); ++c) {
            HPDF_Page_TextOut(page, 30 + 120 * (HPDF_REAL)c, y,
                              t.headers[c].substr(0, 28).c_str());
        }
        HPDF_Page_EndText(page);
        y -= 16;
    }
    for (size_t r = 0; r < t.rows.size() && y > 30; ++r, y -= 14) {
        if (!font) break;
        HPDF_Page_BeginText(page);
        for (size_t c = 0; c < t.rows[r].size() && c < 6; ++c) {
            HPDF_Page_TextOut(page, 30 + 120 * (HPDF_REAL)c, y,
                              t.rows[r][c].substr(0, 30).c_str());
        }
        HPDF_Page_EndText(page);
    }
    HPDF_SaveToFile(pdf, path.string().c_str());
    HPDF_Free(pdf);

    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    std::error_code ec;
    fs::remove(path, ec);
    return ss.str();
}
#endif

} // namespace

json ReportService::progress(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        auto courses = t.query(
            "SELECT c.id, c.title, c.cover, e.progress_pct, e.status, "
            "(SELECT count(*) FROM levels l WHERE l.course_id = c.id) AS levels_total, "
            "(SELECT count(*) FROM level_progress lp JOIN levels l ON l.id = lp.level_id "
            " WHERE l.course_id = c.id AND lp.student_id = $1 AND lp.status = 'completed') AS levels_done, "
            "(SELECT coalesce(sum(lp.stars), 0) FROM level_progress lp JOIN levels l ON l.id = lp.level_id "
            " WHERE l.course_id = c.id AND lp.student_id = $1) AS stars, "
            "(SELECT coalesce(sum(a.time_spent_ms), 0) FROM attempts a JOIN tasks tk ON tk.id = a.task_id "
            " JOIN levels l ON l.id = tk.level_id WHERE l.course_id = c.id AND a.student_id = $1) AS time_ms "
            "FROM course_enrollments e JOIN courses c ON c.id = e.course_id "
            "WHERE e.student_id = $1 ORDER BY e.updated_at DESC",
            {param(ctx.userId)});
        json courseArr = json::array();
        for (const auto& c : courses) {
            json item = c;
            item["title"] = I18n::pick(c["title"], ctx.locale);
            courseArr.push_back(item);
        }
        const int64_t totalTime = t.scalarInt(
            "SELECT coalesce(sum(time_spent_ms), 0) FROM attempts WHERE student_id = $1",
            {param(ctx.userId)});
        const int64_t lessonsDone = t.scalarInt(
            "SELECT count(*) FROM level_progress WHERE student_id = $1 AND status = 'completed'",
            {param(ctx.userId)});
        const int64_t totalStars = t.scalarInt(
            "SELECT coalesce(sum(stars), 0) FROM level_progress WHERE student_id = $1",
            {param(ctx.userId)});
        return json{{"courses", courseArr},
                    {"total_time_ms", totalTime},
                    {"levels_completed", lessonsDone},
                    {"total_stars", totalStars}};
    });
}

json ReportService::xp(const AuthCtx& ctx, int days) {
    days = std::clamp(days, 7, 365);
    return Db::instance().tx([&](Tx& t) {
        auto student = t.one("SELECT total_xp, level FROM students WHERE user_id = $1",
                             {param(ctx.userId)});
        const long long total = student.is_null() ? 0 : student.value("total_xp", 0LL);
        auto progress = svc::xpProgress(total, ctx.locale);

        auto daily = t.query(
            "SELECT d::date::text AS day, coalesce(sum(e.amount), 0) AS xp "
            "FROM generate_series(current_date - ($2::int - 1), current_date, interval '1 day') d "
            "LEFT JOIN xp_events e ON e.student_id = $1 "
            "AND (e.created_at AT TIME ZONE 'Asia/Qyzylorda')::date = d::date AND e.amount > 0 "
            "GROUP BY d ORDER BY d",
            {param(ctx.userId), param(days)});
        auto sources = t.query(
            "SELECT reason, sum(amount) AS xp, count(*) AS events FROM xp_events "
            "WHERE student_id = $1 AND amount > 0 GROUP BY reason ORDER BY xp DESC",
            {param(ctx.userId)});
        return json{{"total_xp", total},
                    {"progress", progress},
                    {"daily", daily},
                    {"sources", sources},
                    {"days", days}};
    });
}

json ReportService::topics(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        auto rows = t.query(
            "SELECT tp.name, tp.code, coalesce(tm.mastery, 0.3) AS mastery, "
            "coalesce(tm.attempts, 0) AS attempts, coalesce(tm.correct, 0) AS correct, "
            "CASE WHEN coalesce(tm.attempts, 0) = 0 THEN 0 "
            "     ELSE round(100.0 * tm.correct / tm.attempts, 1) END AS accuracy, "
            "coalesce(ts.avg_time_ms, 0) AS avg_time_ms "
            "FROM topics tp LEFT JOIN topic_mastery tm ON tm.topic_id = tp.id AND tm.student_id = $1 "
            "LEFT JOIN v_topic_stats ts ON ts.topic_id = tp.id AND ts.student_id = $1 "
            "WHERE tp.id IN (SELECT DISTINCT tk.topic_id FROM tasks tk "
            "  JOIN levels l ON l.id = tk.level_id "
            "  JOIN course_enrollments e ON e.course_id = l.course_id AND e.student_id = $1) "
            "ORDER BY mastery ASC",
            {param(ctx.userId)});
        json out = json::array();
        for (const auto& r : rows) {
            json item = r;
            item["name"] = I18n::pick(r["name"], ctx.locale);
            out.push_back(item);
        }
        return json{{"topics", out}};
    });
}

json ReportService::achievements(const AuthCtx& ctx) {
    auto collection = AchievementService::collection(ctx);
    json byRarity = json::object();
    for (const auto& a : collection["achievements"]) {
        const std::string rarity = a.value("rarity", "common");
        byRarity[rarity] = byRarity.value(rarity, 0) + 1;
    }
    return json{{"unlocked", collection["unlocked"]},
                {"total", collection["total"]},
                {"by_rarity", byRarity},
                {"achievements", collection["achievements"]}};
}

json ReportService::leaderboard(const AuthCtx& ctx) {
    auto global = LeaderboardService::query(ctx, "global", "all", "", "", 20, 0);
    auto weekly = LeaderboardService::query(ctx, "weekly", "week", "", "", 10, 0);
    return json{{"global", global}, {"weekly", weekly}, {"me", global["me"]}};
}

json ReportService::heatmap(const AuthCtx& ctx, int days) {
    days = std::clamp(days, 30, 365);
    return Db::instance().tx([&](Tx& t) {
        auto rows = t.query(
            "SELECT d::date::text AS day, coalesce(sum(e.amount), 0) AS xp, "
            "coalesce(max(sl.goal_met), false) AS goal_met "
            "FROM generate_series(current_date - ($2::int - 1), current_date, interval '1 day') d "
            "LEFT JOIN xp_events e ON e.student_id = $1 "
            "AND (e.created_at AT TIME ZONE 'Asia/Qyzylorda')::date = d::date "
            "LEFT JOIN streak_log sl ON sl.student_id = $1 AND sl.log_date = d::date "
            "GROUP BY d ORDER BY d",
            {param(ctx.userId), param(days)});
        return json{{"days", rows}, {"window", days}};
    });
}

json ReportService::overview(const AuthCtx& ctx) {
    return json{{"progress", progress(ctx)},
                {"xp", xp(ctx, 30)},
                {"topics", topics(ctx)},
                {"achievements", achievements(ctx)},
                {"heatmap", heatmap(ctx, 120)}};
}

ReportFile ReportService::exportReport(const AuthCtx& ctx, const std::string& type,
                                       const std::string& format) {
    ReportFile file;
    std::vector<Table> tables;

    if (type == "xp") {
        auto data = xp(ctx, 30);
        json rows = json::array();
        for (const auto& d : data["daily"]) {
            rows.push_back({{"day", d["day"]}, {"xp", d["xp"]}});
        }
        tables.push_back(toTable("XP", rows, {"day", "xp"}));
        file.filename = "lumina_xp";
    } else if (type == "topics") {
        auto data = topics(ctx)["topics"];
        tables.push_back(toTable("Topics", data, {"name", "mastery", "attempts", "accuracy"}));
        file.filename = "lumina_topics";
    } else if (type == "achievements") {
        auto data = achievements(ctx)["achievements"];
        tables.push_back(toTable("Achievements", data, {"title", "category", "rarity", "awarded"}));
        file.filename = "lumina_achievements";
    } else if (type == "leaderboard") {
        auto data = leaderboard(ctx)["global"]["entries"];
        tables.push_back(toTable("Leaderboard", data, {"rank", "nickname", "level", "score"}));
        file.filename = "lumina_leaderboard";
    } else {
        auto data = progress(ctx)["courses"];
        tables.push_back(toTable("Progress", data,
                                 {"title", "progress_pct", "levels_done", "levels_total", "stars"}));
        file.filename = "lumina_progress";
    }

    if (format == "xlsx") {
#if defined(LUMINA_HAVE_XLSX)
        file.body = tableXlsx(tables);
        file.contentType = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
        file.filename += ".xlsx";
        file.binary = true;
#else
        throw badRequest("XLSX export is not available in this build");
#endif
    } else if (format == "pdf") {
#if defined(LUMINA_HAVE_HPDF)
        file.body = tablePdf(tables.front());
        file.contentType = "application/pdf";
        file.filename += ".pdf";
        file.binary = true;
#else
        throw badRequest("PDF export is not available in this build");
#endif
    } else {
        file.body = tableCsv(tables.front());
        file.contentType = "text/csv; charset=utf-8";
        file.filename += ".csv";
    }
    return file;
}

} // namespace lumina



