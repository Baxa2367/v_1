#include "core/config.h"
#include "core/crypto.h"
#include "core/log.h"
#include "core/util.h"
#include "db/db.h"
#include "http/filters.h"
#include "http/http_utils.h"
#include "jobs/worker.h"
#include "sandbox/sandbox.h"
#include <drogon/drogon.h>
#include <filesystem>

using namespace lumina;

// Static libraries drop unreferenced translation units; these references keep
// Drogon controller registration constructors in the final binary.
extern "C" void lumina_link_auth_controller();
extern "C" void lumina_link_learning_controller();
extern "C" void lumina_link_social_controller();
extern "C" void lumina_link_staff_controller();
extern "C" void lumina_link_system_controller();

int main(int argc, char** argv) {
    lumina_link_auth_controller();
    lumina_link_learning_controller();
    lumina_link_social_controller();
    lumina_link_staff_controller();
    lumina_link_system_controller();
    Config::instance().load();
    Log::init(Config::instance().get("LOG_LEVEL", "info"));
    Crypto::init();
    Sandbox::setCompiler(Config::instance().get("SANDBOX_GXX", "g++"));

    try {
        Db::instance().init(Config::instance().databaseUrl(), 10);
    } catch (const std::exception& e) {
        Log::get()->critical("database connection failed: {}", e.what());
        return 1;
    }

    const int port = Config::instance().port();
    const std::string publicDir = Config::instance().publicDir();
    const int rateLimit = Config::instance().getInt("RATE_LIMIT_PER_MIN", 240);

    auto& app = drogon::app();
    app.setLogLevel(trantor::Logger::kWarn);
    app.setThreadNum(4);
    app.addListener("0.0.0.0", static_cast<uint16_t>(port));
    app.setIdleConnectionTimeout(75);

    if (std::filesystem::exists(publicDir)) {
        app.setDocumentRoot(publicDir);
        const auto index = publicDir + "/index.html";
        if (std::filesystem::exists(index)) {
            auto page = drogon::HttpResponse::newFileResponse(index);
            app.setCustom404Page(page, false);
        }
    }

    app.registerPreRoutingAdvice([rateLimit](const drogon::HttpRequestPtr& req,
                                             drogon::AdviceCallback&& acb,
                                             drogon::AdviceChainCallback&& accb) {
        const std::string ip = http::clientIp(req);
        if (req->path() != "/healthz" && !RateLimiter::instance().allow(ip, rateLimit)) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k429TooManyRequests);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error":{"code":"rate_limited","message":"Too many requests"}})");
            acb(resp);
            return;
        }
        if (req->method() == drogon::Options) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            acb(resp);
            return;
        }
        req->attributes()->insert("startMs", unixNow() * 1000);
        accb();
    });

    app.registerPostHandlingAdvice([](const drogon::HttpRequestPtr& req,
                                      const drogon::HttpResponsePtr& resp) {
        const std::string origin = Config::instance().get("CORS_ORIGIN", "*");
        resp->addHeader("Access-Control-Allow-Origin", origin);
        resp->addHeader("Vary", "Origin");
        resp->addHeader("Access-Control-Allow-Methods", "GET,POST,PUT,PATCH,DELETE,OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers",
                        "Authorization,Content-Type,Accept,Accept-Language,X-Locale");
        resp->addHeader("Access-Control-Max-Age", "86400");
        resp->addHeader("X-Content-Type-Options", "nosniff");
        resp->addHeader("X-Frame-Options", "SAMEORIGIN");
        resp->addHeader("Referrer-Policy", "strict-origin-when-cross-origin");
        if (req->path().rfind("/api/", 0) == 0) {
            Log::get()->info("{} {} -> {}", req->methodString(), req->path(),
                             static_cast<int>(resp->statusCode()));
        }
    });

    jobs::Worker::start(60);

    Log::get()->info("Lumina {} listening on http://127.0.0.1:{}", LUMINA_VERSION, port);
    Log::get()->info("static root: {}", publicDir);
    app.run();
    jobs::Worker::stop();
    return 0;
}


