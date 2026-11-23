#include "db/db.h"
#include "core/log.h"
#include "core/util.h"
#include <chrono>
#include <thread>

namespace lumina {

std::optional<std::string> param(const std::string& v) { return v; }
std::optional<std::string> param(const char* v) { return std::string(v); }
std::optional<std::string> param(int v) { return std::to_string(v); }
std::optional<std::string> param(int64_t v) { return std::to_string(v); }
std::optional<std::string> param(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.10g", v);
    return std::string(buf);
}
std::optional<std::string> param(bool v) { return std::string(v ? "true" : "false"); }
std::optional<std::string> paramJson(const json& v) { return v.dump(); }
std::optional<std::string> param(const json& v) {
    if (v.is_null()) return std::nullopt;
    if (v.is_string()) return v.get<std::string>();
    if (v.is_boolean()) return std::string(v.get<bool>() ? "true" : "false");
    if (v.is_number_integer()) return std::to_string(v.get<long long>());
    if (v.is_number_unsigned()) return std::to_string(v.get<unsigned long long>());
    if (v.is_number_float()) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.10g", v.get<double>());
        return std::string(buf);
    }
    return v.dump();
}

namespace {

pqxx::params toPqxx(const Params& params) {
    pqxx::params p;
    for (const auto& v : params) {
        if (v) p.append(*v);
        else p.append();
    }
    return p;
}

std::string normalizeTs(const std::string& s) {
    // "2026-10-29 14:50:00+00" -> "2026-10-29T14:50:00Z"
    if (s.size() < 19 || s[10] != ' ') return s;
    std::string out = s;
    out[10] = 'T';
    if (out.size() >= 3 && out.compare(out.size() - 3, 3, "+00") == 0) {
        out.replace(out.size() - 3, 3, "Z");
    }
    return out;
}

} // namespace

// ---------------- Tx ----------------

json Tx::query(const std::string& sql, const Params& params) {
    try {
        auto p = toPqxx(params);
        return Db::resultToJson(w_.exec_params(sql, p));
    } catch (const std::exception& e) {
        translatePgException(e);
        throw;
    }
}

json Tx::one(const std::string& sql, const Params& params) {
    auto arr = query(sql, params);
    if (arr.empty()) return nullptr;
    return arr[0];
}

int64_t Tx::exec(const std::string& sql, const Params& params) {
    try {
        auto p = toPqxx(params);
        auto res = w_.exec_params(sql, p);
        return static_cast<int64_t>(res.affected_rows());
    } catch (const std::exception& e) {
        translatePgException(e);
        throw;
    }
}

int64_t Tx::execRaw(const std::string& sql) {
    try {
        auto res = w_.exec(sql);
        return static_cast<int64_t>(res.affected_rows());
    } catch (const std::exception& e) {
        translatePgException(e);
        throw;
    }
}

std::optional<std::string> Tx::scalar(const std::string& sql, const Params& params) {
    auto r = one(sql, params);
    if (r.is_null() || !r.is_object() || r.empty()) return std::nullopt;
    const auto& v = r.begin().value();
    if (v.is_null()) return std::nullopt;
    if (v.is_string()) return v.get<std::string>();
    return v.dump();
}

int64_t Tx::scalarInt(const std::string& sql, const Params& params, int64_t def) {
    auto s = scalar(sql, params);
    if (!s) return def;
    try {
        return std::stoll(*s);
    } catch (...) {
        return def;
    }
}

double Tx::scalarDouble(const std::string& sql, const Params& params, double def) {
    auto s = scalar(sql, params);
    if (!s) return def;
    double v = def;
    return tryParseNumber(*s, v) ? v : def;
}

bool Tx::scalarBool(const std::string& sql, const Params& params, bool def) {
    auto s = scalar(sql, params);
    if (!s) return def;
    return *s == "t" || *s == "true" || *s == "1";
}

// ---------------- Db ----------------

Db& Db::instance() {
    static Db db;
    return db;
}

void Db::init(const std::string& url, int poolSize) {
    std::lock_guard lock(mutex_);
    if (!pool_.empty()) return;
    for (int i = 0; i < poolSize; ++i) {
        auto c = std::make_shared<Conn>();
        c->conn = std::make_unique<pqxx::connection>(url);
        pool_.push_back(c);
    }
    Log::get()->info("database pool ready ({} connections)", poolSize);
}

void Db::close() {
    std::lock_guard lock(mutex_);
    closed_ = true;
    pool_.clear();
    cv_.notify_all();
}

std::shared_ptr<Db::Conn> Db::acquire() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] {
        if (closed_) return true;
        for (auto& c : pool_) {
            if (!c->busy) return true;
        }
        return false;
    });
    if (closed_) throw internal("Database is shutting down");
    for (auto& c : pool_) {
        if (!c->busy) {
            c->busy = true;
            return c;
        }
    }
    throw internal("No database connection available");
}

void Db::release(const std::shared_ptr<Conn>& conn) {
    {
        std::lock_guard lock(mutex_);
        conn->busy = false;
    }
    cv_.notify_one();
}

json Db::query(const std::string& sql, const Params& params) {
    return tx([&](Tx& t) { return t.query(sql, params); });
}

json Db::rowToJson(const pqxx::row_ref& row) {
    json obj = json::object();
    for (int i = 0; i < row.size(); ++i) {
        const auto& f = row[i];
        const std::string name = row[i].name();
        if (f.is_null()) {
            obj[name] = nullptr;
            continue;
        }
        switch (f.type()) {
            case 16: // bool
                obj[name] = f.as<bool>();
                break;
            case 20: case 21: case 23: case 26: // int8 int2 int4 oid
                obj[name] = f.as<int64_t>();
                break;
            case 700: case 701: case 1700: // float4 float8 numeric
                obj[name] = f.as<double>();
                break;
            case 114: case 3802: { // json jsonb
                try {
                    obj[name] = json::parse(f.as<std::string>());
                } catch (...) {
                    obj[name] = f.as<std::string>();
                }
                break;
            }
            case 1114: case 1184: // timestamp / timestamptz
                obj[name] = normalizeTs(f.as<std::string>());
                break;
            default:
                obj[name] = f.as<std::string>();
                break;
        }
    }
    return obj;
}

json Db::resultToJson(const pqxx::result& res) {
    json arr = json::array();
    for (const auto& row : res) arr.push_back(rowToJson(row));
    return arr;
}

void translatePgException(const std::exception& e) {
    const std::string what = e.what();
    if (auto* sqlErr = dynamic_cast<const pqxx::sql_error*>(&e)) {
        const std::string code(sqlErr->sqlstate());
        if (code == "23505") throw conflict("Duplicate value", {{"sqlstate", code}});
        if (code == "23503") throw badRequest("Referenced record does not exist", {{"sqlstate", code}});
        if (code == "23514") throw unprocessable("Constraint violation", {{"sqlstate", code}});
        if (code == "22P02") throw badRequest("Invalid input syntax", {{"sqlstate", code}});
        if (code == "40001" || code == "40P01") {
            throw ApiError{409, "retry", "Concurrent update, please retry", {{"sqlstate", code}}};
        }
        Log::get()->error("sql error {}: {} | query: {}", code, what, sqlErr->query());
        throw internal("Database error");
    }
    if (dynamic_cast<const pqxx::broken_connection*>(&e)) {
        throw ApiError{503, "database_unavailable", "Database is unavailable"};
    }
    Log::get()->error("database exception: {}", what);
    throw internal("Database error");
}

} // namespace lumina



