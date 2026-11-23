#include "core/config.h"
#include "core/crypto.h"
#include "core/log.h"
#include "core/util.h"
#include "db/db.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>

using namespace lumina;
namespace fs = std::filesystem;

namespace {

std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + p.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

struct Migration {
    int version = 0;
    std::string name;
    fs::path up;
    fs::path down;
};

std::vector<Migration> discover(const std::string& dir) {
    std::vector<Migration> out;
    std::regex re(R"(^(\d{4})_([a-zA-Z0-9_\-]+)\.up\.sql$)");
    if (!fs::exists(dir)) throw std::runtime_error("migrations dir not found: " + dir);
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        const std::string fname = entry.path().filename().string();
        std::smatch m;
        if (!std::regex_match(fname, m, re)) continue;
        Migration mig;
        mig.version = std::stoi(m[1].str());
        mig.name = m[2].str();
        mig.up = entry.path();
        mig.down = entry.path().parent_path() / (m[1].str() + "_" + m[2].str() + ".down.sql");
        out.push_back(std::move(mig));
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return a.version < b.version;
    });
    for (size_t i = 1; i < out.size(); ++i) {
        if (out[i].version == out[i - 1].version) {
            throw std::runtime_error("duplicate migration version " + std::to_string(out[i].version));
        }
    }
    return out;
}

void ensureTable() {
    Db::instance().tx([](Tx& t) {
        t.execRaw(R"(
            CREATE TABLE IF NOT EXISTS schema_migrations (
                version    int PRIMARY KEY,
                name       text NOT NULL,
                checksum   text NOT NULL DEFAULT '',
                applied_at timestamptz NOT NULL DEFAULT now()
            ))");
    });
}

std::map<int, std::string> applied() {
    std::map<int, std::string> out;
    for (const auto& row : Db::instance().query(
             "SELECT version, checksum FROM schema_migrations ORDER BY version")) {
        out[row["version"].get<int>()] = row["checksum"].get<std::string>();
    }
    return out;
}

void applyOne(const Migration& m) {
    const std::string sql = readFile(m.up);
    const std::string checksum = Crypto::sha256Hex(sql);
    Db::instance().tx([&](Tx& t) {
        t.execRaw("SELECT pg_advisory_xact_lock(721001)");
        t.execRaw(sql);
        t.exec("INSERT INTO schema_migrations (version, name, checksum) VALUES ($1, $2, $3)",
               {param(m.version), param(m.name), param(checksum)});
    });
    std::cout << "applied  " << m.version << "_" << m.name << "\n";
}

void revertOne(const Migration& m) {
    if (!fs::exists(m.down)) {
        throw std::runtime_error("missing down migration for " + m.name);
    }
    const std::string sql = readFile(m.down);
    Db::instance().tx([&](Tx& t) {
        t.execRaw("SELECT pg_advisory_xact_lock(721001)");
        t.execRaw(sql);
        t.exec("DELETE FROM schema_migrations WHERE version = $1", {param(m.version)});
    });
    std::cout << "reverted " << m.version << "_" << m.name << "\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        Config::instance().load();
        Log::init("info", "logs/migrator.log");
        Crypto::init();
        Db::instance().init(Config::instance().databaseUrl(), 2);
        ensureTable();

        const auto migrations = discover(Config::instance().migrationsDir());
        const auto done = applied();

        std::string cmd = argc > 1 ? argv[1] : "up";

        if (cmd == "status") {
            for (const auto& m : migrations) {
                bool isDone = done.count(m.version) > 0;
                std::cout << (isDone ? "[x] " : "[ ] ") << m.version << "_" << m.name << "\n";
            }
            return 0;
        }

        if (cmd == "down") {
            int target = argc > 2 ? std::stoi(argv[2]) : 0;
            auto sorted = migrations;
            std::reverse(sorted.begin(), sorted.end());
            for (const auto& m : sorted) {
                if (!done.count(m.version)) continue;
                if (m.version <= target) continue;
                revertOne(m);
            }
            return 0;
        }

        int appliedCount = 0;
        for (const auto& m : migrations) {
            auto it = done.find(m.version);
            if (it != done.end()) {
                const std::string actual = Crypto::sha256Hex(readFile(m.up));
                if (!it->second.empty() && it->second != actual) {
                    Log::get()->warn("migration {} checksum changed since apply", m.name);
                }
                continue;
            }
            applyOne(m);
            ++appliedCount;
        }
        std::cout << (appliedCount ? "migrations applied: " + std::to_string(appliedCount)
                                   : "database is up to date")
                  << "\n";
        return 0;
    } catch (const ApiError& e) {
        std::cerr << "migration error: " << e.code() << " " << e.message() << "\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "migration failed: " << e.what() << "\n";
        return 1;
    }
}
