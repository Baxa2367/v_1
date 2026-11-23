#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace lumina {

// Minimal .env / environment configuration loader.
class Config {
public:
    static Config& instance();

    void load(const std::string& envFile = ".env");
    std::string get(const std::string& key, const std::string& def = "") const;
    int getInt(const std::string& key, int def) const;
    bool getBool(const std::string& key, bool def) const;

    const std::string& databaseUrl() const { return dbUrl_; }
    const std::string& timezone() const { return tz_; }
    const std::string& migrationsDir() const { return migrationsDir_; }
    const std::string& seedsDir() const { return seedsDir_; }
    const std::string& publicDir() const { return publicDir_; }
    int port() const { return port_; }

private:
    Config() = default;
    void setDefault(const std::string& k, const std::string& v) {
        if (!values_.count(k)) values_[k] = v;
    }

    std::unordered_map<std::string, std::string> values_;
    std::string dbUrl_;
    std::string tz_;
    std::string migrationsDir_;
    std::string seedsDir_;
    std::string publicDir_;
    int port_ = 8080;
};

} // namespace lumina
