#include "http/http_utils.h"
#include "core/config.h"
#include "core/crypto.h"
#include "core/i18n.h"
#include "core/log.h"
#include <drogon/drogon.h>
#include <fstream>
#include <set>

namespace lumina::http {

HttpResponsePtr ok(const json& data, int status) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(static_cast<drogon::HttpStatusCode>(status));
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setBody(json{{"data", data}}.dump());
    return resp;
}

HttpResponsePtr created(const json& data) { return ok(data, 201); }

HttpResponsePtr noContent() {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k204NoContent);
    return resp;
}

HttpResponsePtr fail(const ApiError& e) {
    if (e.status() >= 500) LOG_ERROR << "api error " << e.code() << ": " << e.message();
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(static_cast<drogon::HttpStatusCode>(e.status()));
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setBody(e.toJson().dump());
    return resp;
}

HttpResponsePtr fail(int status, const std::string& code, const std::string& message) {
    return fail(ApiError{status, code, message});
}

HttpResponsePtr binary(std::string body, const std::string& contentType,
                       const std::string& filename) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeString(contentType);
    resp->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    resp->setBody(std::move(body));
    return resp;
}

HttpResponsePtr redirect(const std::string& url) {
    auto resp = drogon::HttpResponse::newRedirectionResponse(url);
    return resp;
}

AuthCtx authCtx(const HttpRequestPtr& req) {
    AuthCtx ctx;
    auto attrs = req->attributes();
    if (!attrs) return ctx;
    ctx.userId = attrs->get<std::string>("userId");
    ctx.role = attrs->get<std::string>("role");
    ctx.locale = attrs->get<std::string>("locale");
    ctx.displayName = attrs->get<std::string>("displayName");
    return ctx;
}

std::string pathParam(const HttpRequestPtr& req, const std::string& key) {
    const std::string fromQuery = req->getParameter(key);
    if (!fromQuery.empty()) return fromQuery;
    const std::string fromAttr = req->attributes()->get<std::string>(key);
    if (!fromAttr.empty()) return fromAttr;

    // Drogon keeps named route captures out of the parameter map, so resolve
    // them from the path shape: the value always follows its resource segment.
    std::vector<std::string> parts;
    std::string cur;
    for (char c : req->path()) {
        if (c == '/') {
            if (!cur.empty()) parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) parts.push_back(cur);
    if (parts.empty()) return {};

    if (key == "tier") return parts.back();

    static const std::set<std::string> resources = {
        "courses", "levels", "tasks", "teams", "tournaments", "seasons", "quests",
        "notifications", "users", "anticheat", "achievements", "topics", "shop",
        "inventory"};
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        if (resources.count(parts[i])) return parts[i + 1];
    }
    return parts.back();
}

std::string clientIp(const HttpRequestPtr& req) {
    auto forwarded = req->getHeader("x-forwarded-for");
    if (!forwarded.empty()) {
        auto comma = forwarded.find(',');
        return forwarded.substr(0, comma);
    }
    return req->peerAddr().toIp();
}

std::string localeOf(const HttpRequestPtr& req, const AuthCtx* ctx) {
    if (req->getHeader("x-locale").size() == 2) {
        return I18n::normalize(req->getHeader("x-locale"));
    }
    if (ctx && !ctx->locale.empty()) return ctx->locale;
    return I18n::normalize(req->getHeader("accept-language"));
}

json parseBody(const HttpRequestPtr& req) {
    if (req->body().empty()) return json::object();
    try {
        auto j = json::parse(req->body());
        if (!j.is_object()) throw std::runtime_error("body must be an object");
        return j;
    } catch (const std::exception&) {
        throw badRequest("Malformed JSON body");
    }
}

void requireStudent(const AuthCtx& ctx) {
    if (ctx.role != "student") throw forbidden("Student account required");
}
void requireTeacher(const AuthCtx& ctx) {
    if (ctx.role != "teacher" && ctx.role != "admin") throw forbidden("Teacher account required");
}
void requireAdmin(const AuthCtx& ctx) {
    if (ctx.role != "admin") throw forbidden("Admin account required");
}

} // namespace lumina::http




