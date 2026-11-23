#pragma once
#include "core/config.h"
#include "core/crypto.h"
#include "core/errors.h"
#include "core/util.h"
#include <drogon/drogon.h>
#include <mutex>
#include <string>
#include <unordered_map>

namespace lumina {

using drogon::FilterCallback;
using drogon::FilterChainCallback;

// JWT authentication filter. Puts userId / role / locale into request attributes.
class AuthFilter : public drogon::HttpFilter<AuthFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req, FilterCallback&& fcb,
                  FilterChainCallback&& ccb) override {
        try {
            const std::string header = req->getHeader("authorization");
            if (header.empty() || !startsWith(toLower(header), "bearer ")) {
                fcb(failJson(401, "unauthorized", "Authentication required"));
                return;
            }
            const std::string token = trimCopy(header.substr(7));
            auto claims = Crypto::verifyJwt(token, Config::instance().get("JWT_SECRET"));
            if (claims.value("typ", "") != "access") throw unauthorized("Wrong token type");
            auto attrs = req->attributes();
            attrs->insert("userId", claims.value("sub", std::string()));
            attrs->insert("role", claims.value("role", std::string()));
            attrs->insert("locale", claims.value("locale", std::string("kk")));
            attrs->insert("displayName", claims.value("name", std::string()));
            ccb();
        } catch (const ApiError& e) {
            fcb(failJson(e.status(), e.code(), e.message()));
        } catch (...) {
            fcb(failJson(401, "unauthorized", "Invalid token"));
        }
    }

private:
    static drogon::HttpResponsePtr failJson(int status, const std::string& code,
                                            const std::string& message) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(static_cast<drogon::HttpStatusCode>(status));
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->setBody(json{{"error", {{"code", code}, {"message", message}}}}.dump());
        return resp;
    }
};

class TeacherFilter : public drogon::HttpFilter<TeacherFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req, FilterCallback&& fcb,
                  FilterChainCallback&& ccb) override {
        const std::string role = req->attributes()->get<std::string>("role");
        if (role != "teacher" && role != "admin") {
            fcb(AuthFilter_fail(403, "forbidden", "Teacher account required"));
            return;
        }
        ccb();
    }
    static drogon::HttpResponsePtr AuthFilter_fail(int status, const std::string& code,
                                                  const std::string& message) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(static_cast<drogon::HttpStatusCode>(status));
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->setBody(json{{"error", {{"code", code}, {"message", message}}}}.dump());
        return resp;
    }
};

class AdminFilter : public drogon::HttpFilter<AdminFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req, FilterCallback&& fcb,
                  FilterChainCallback&& ccb) override {
        const std::string role = req->attributes()->get<std::string>("role");
        if (role != "admin") {
            fcb(TeacherFilter::AuthFilter_fail(403, "forbidden", "Admin account required"));
            return;
        }
        ccb();
    }
};

// Simple in-memory token bucket (per key, per minute).
class RateLimiter {
public:
    static RateLimiter& instance();
    bool allow(const std::string& key, int perMinute);

private:
    struct Bucket {
        double tokens;
        int64_t updatedMs;
    };
    std::mutex mutex_;
    std::unordered_map<std::string, Bucket> buckets_;
};

} // namespace lumina

