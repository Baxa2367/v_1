#pragma once
#include "core/errors.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <type_traits>
#include <utility>

namespace lumina {

using Params = std::vector<std::optional<std::string>>;

std::optional<std::string> param(const std::string& v);
std::optional<std::string> param(const char* v);
std::optional<std::string> param(int v);
std::optional<std::string> param(int64_t v);
std::optional<std::string> param(double v);
std::optional<std::string> param(bool v);
std::optional<std::string> param(const json& v);
std::optional<std::string> paramJson(const json& v);

template <typename T,
          std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, int> &&
                               !std::is_same_v<T, int64_t> && !std::is_same_v<T, bool>,
                           int> = 0>
std::optional<std::string> param(T v) {
    return std::to_string(v);
}

class Tx {
public:
    explicit Tx(pqxx::work& w) : w_(w) {}

    json query(const std::string& sql, const Params& params = {});
    json one(const std::string& sql, const Params& params = {});
    int64_t exec(const std::string& sql, const Params& params = {});
    // Simple query protocol: allows several statements in one string (migrations).
    int64_t execRaw(const std::string& sql);
    std::optional<std::string> scalar(const std::string& sql, const Params& params = {});
    int64_t scalarInt(const std::string& sql, const Params& params = {}, int64_t def = 0);
    double scalarDouble(const std::string& sql, const Params& params = {}, double def = 0.0);
    bool scalarBool(const std::string& sql, const Params& params = {}, bool def = false);

    pqxx::work& raw() { return w_; }

private:
    pqxx::work& w_;
};

class Db {
public:
    static Db& instance();

    void init(const std::string& url, int poolSize = 8);
    void close();

    template <typename F>
    auto tx(F&& fn) -> decltype(fn(std::declval<Tx&>())) {
        using Result = decltype(fn(std::declval<Tx&>()));
        for (int attempt = 0; attempt < 4; ++attempt) {
            auto conn = acquire();
            try {
                pqxx::work w(*conn->conn);
                Tx t(w);
                if constexpr (std::is_void_v<Result>) {
                    fn(t);
                    w.commit();
                    release(conn);
                    return;
                } else {
                    Result r = fn(t);
                    w.commit();
                    release(conn);
                    return r;
                }
            } catch (pqxx::serialization_failure&) {
                release(conn);
                if (attempt == 3) throw;
            } catch (...) {
                release(conn);
                throw;
            }
        }
        throw internal("Transaction retry budget exhausted");
    }

    // Single-statement convenience (auto-commit).
    json query(const std::string& sql, const Params& params = {});

    static json rowToJson(const pqxx::row& row);
    static json resultToJson(const pqxx::result& res);

private:
    struct Conn {
        std::unique_ptr<pqxx::connection> conn;
        bool busy = false;
    };
    std::shared_ptr<Conn> acquire();
    void release(const std::shared_ptr<Conn>& conn);

    std::vector<std::shared_ptr<Conn>> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool closed_ = false;
};

// Maps pqxx errors to ApiError with sane HTTP codes.
void translatePgException(const std::exception& e);

} // namespace lumina



