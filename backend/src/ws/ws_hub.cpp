#include "ws/ws_hub.h"
#include "core/config.h"
#include "core/crypto.h"
#include "core/log.h"
#include "core/util.h"
#include <algorithm>

namespace lumina {

Hub& Hub::instance() {
    static Hub hub;
    return hub;
}

void Hub::add(const std::string& userId, const drogon::WebSocketConnectionPtr& conn) {
    std::lock_guard lock(mutex_);
    byUser_[userId].push_back(conn);
}

void Hub::remove(const drogon::WebSocketConnectionPtr& conn) {
    std::lock_guard lock(mutex_);
    for (auto it = byUser_.begin(); it != byUser_.end();) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), conn), vec.end());
        if (vec.empty()) it = byUser_.erase(it);
        else ++it;
    }
}

void Hub::send(const std::string& userId, const json& event) {
    std::vector<drogon::WebSocketConnectionPtr> targets;
    {
        std::lock_guard lock(mutex_);
        auto it = byUser_.find(userId);
        if (it == byUser_.end()) return;
        targets = it->second;
    }
    const std::string payload = event.dump();
    for (const auto& conn : targets) {
        if (conn) conn->send(payload);
    }
}

void Hub::broadcast(const json& event) {
    std::vector<drogon::WebSocketConnectionPtr> targets;
    {
        std::lock_guard lock(mutex_);
        for (const auto& [user, vec] : byUser_) {
            targets.insert(targets.end(), vec.begin(), vec.end());
        }
    }
    const std::string payload = event.dump();
    for (const auto& conn : targets) {
        if (conn) conn->send(payload);
    }
}

size_t Hub::online() const {
    std::lock_guard lock(mutex_);
    size_t total = 0;
    for (const auto& [user, vec] : byUser_) total += vec.size();
    return total;
}

void WsController::handleNewConnection(const drogon::HttpRequestPtr& req,
                                       const drogon::WebSocketConnectionPtr& conn) {
    const std::string token = req->getParameter("token");
    if (token.empty()) {
        conn->shutdown();
        return;
    }
    try {
        auto claims = Crypto::verifyJwt(token, Config::instance().get("JWT_SECRET"));
        const std::string userId = claims.value("sub", "");
        conn->setContext(std::make_shared<std::string>(userId));
        Hub::instance().add(userId, conn);
        conn->send(json{{"type", "connected"},
                        {"user_id", userId},
                        {"server_time", isoNow()}}
                       .dump());
        Log::get()->info("ws connected: user={} online={}", userId, Hub::instance().online());
    } catch (const std::exception&) {
        conn->shutdown();
    }
}

void WsController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                                    std::string&& message,
                                    const drogon::WebSocketMessageType& type) {
    if (type == drogon::WebSocketMessageType::Ping) return;
    try {
        auto msg = json::parse(message);
        if (msg.value("type", "") == "ping") {
            conn->send(json{{"type", "pong"}, {"ts", isoNow()}}.dump());
        }
    } catch (...) {
    }
}

void WsController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) {
    Hub::instance().remove(conn);
}

} // namespace lumina
