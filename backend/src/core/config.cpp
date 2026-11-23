#include "core/config.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace lumina {

namespace {
std::string trim(std::string s) {
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}
} // namespace

Config& Config::instance() {
    static Config cfg;
    return cfg;
}

void Config::load(const std::string& envFile) {
    const char* envPath = std::getenv("LUMINA_ENV_FILE");
    const std::string path = envPath ? envPath : envFile;
    std::ifstream in(path);
    if (in) {
        std::string line;
        while (std::getline(in, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = trim(line.substr(0, eq));
            std::string val = trim(line.substr(eq + 1));
            if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') ||
                                    (val.front() == '\'' && val.back() == '\''))) {
                val = val.substr(1, val.size() - 2);
            }
            values_[key] = val;
        }
    }

    // Environment variables win over .env file (12-factor).
    static const char* known[] = {
        "DATABASE_URL", "PORT", "APP_TIMEZONE", "JWT_SECRET", "ACCESS_TTL_MIN",
        "REFRESH_TTL_DAYS", "MIGRATIONS_DIR", "SEEDS_DIR", "PUBLIC_DIR",
        "ANTHROPIC_API_KEY", "ANTHROPIC_MODEL", "REDIS_URL", "SANDBOX_GXX",
        "CORS_ORIGIN", "LOG_LEVEL", "RATE_LIMIT_PER_MIN", "DEMO_MODE",
        "BOOTSTRAP_ADMIN_EMAIL", "BOOTSTRAP_ADMIN_PASSWORD"};
    for (const char* k : known) {
        if (const char* v = std::getenv(k); v && *v) values_[k] = v;
    }

    setDefault("PORT", "8080");
    setDefault("APP_TIMEZONE", "Asia/Qyzylorda");
    setDefault("JWT_SECRET", "dev-insecure-change-me-please-32bytes-min");
    setDefault("ACCESS_TTL_MIN", "60");
    setDefault("REFRESH_TTL_DAYS", "30");
    setDefault("MIGRATIONS_DIR", "../db/migrations");
    setDefault("SEEDS_DIR", "../seeds");
    setDefault("PUBLIC_DIR", "../frontend/dist");
    setDefault("LOG_LEVEL", "info");
    setDefault("RATE_LIMIT_PER_MIN", "240");
    setDefault("CORS_ORIGIN", "*");
    setDefault("DEMO_MODE", "1");

    dbUrl_ = get("DATABASE_URL",
                 "postgresql://lumina:lumina@127.0.0.1:5433/lumina");
    tz_ = get("APP_TIMEZONE");
    migrationsDir_ = get("MIGRATIONS_DIR");
    seedsDir_ = get("SEEDS_DIR");
    publicDir_ = get("PUBLIC_DIR");
    port_ = getInt("PORT", 8080);
}

std::string Config::get(const std::string& key, const std::string& def) const {
    auto it = values_.find(key);
    return it == values_.end() ? def : it->second;
}

int Config::getInt(const std::string& key, int def) const {
    auto it = values_.find(key);
    if (it == values_.end() || it->second.empty()) return def;
    try {
        return std::stoi(it->second);
    } catch (...) {
        return def;
    }
}

bool Config::getBool(const std::string& key, bool def) const {
    auto it = values_.find(key);
    if (it == values_.end()) return def;
    std::string v = it->second;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return std::tolower(c); });
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

} // namespace lumina
