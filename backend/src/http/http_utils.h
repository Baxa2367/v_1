#pragma once
#include "core/errors.h"
#include "services/common.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <functional>
#include <string>
#include "core/log.h"

namespace lumina::http {

using drogon::HttpRequestPtr;
using drogon::HttpResponsePtr;
using Callback = std::function<void(const HttpResponsePtr&)>;

HttpResponsePtr ok(const json& data, int status = 200);
HttpResponsePtr created(const json& data);
HttpResponsePtr noContent();
HttpResponsePtr fail(const ApiError& e);
HttpResponsePtr fail(int status, const std::string& code, const std::string& message);
HttpResponsePtr binary(std::string body, const std::string& contentType, const std::string& filename);
HttpResponsePtr redirect(const std::string& url);

AuthCtx authCtx(const HttpRequestPtr& req);
std::string pathParam(const HttpRequestPtr& req, const std::string& key);
std::string clientIp(const HttpRequestPtr& req);
std::string localeOf(const HttpRequestPtr& req, const AuthCtx* ctx = nullptr);

// Runs `fn`, converting exceptions into a uniform error response.
template <typename F>
void run(const Callback& cb, F&& fn) {
    try {
        cb(ok(fn()));
    } catch (const ApiError& e) {
        cb(fail(e));
    } catch (const std::exception& e) {
        cb(fail(500, "internal_error", "Internal server error"));
        Log::get()->error("unhandled exception: {}", e.what());
    }
}

json parseBody(const HttpRequestPtr& req);
void requireStudent(const AuthCtx& ctx);
void requireTeacher(const AuthCtx& ctx);
void requireAdmin(const AuthCtx& ctx);

} // namespace lumina::http



