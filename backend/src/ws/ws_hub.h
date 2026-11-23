#pragma once
#include <drogon/WebSocketConnection.h>
#include <drogon/WebSocketController.h>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace lumina {

using json = nlohmann::json;

class Hub {
public:
    static Hub& instance();

    void add(const std::string& userId, const drogon::WebSocketConnectionPtr& conn);
    void remove(const drogon::WebSocketConnectionPtr& conn);
    void send(const std::string& userId, const json& event);
    void broadcast(const json& event);
    size_t online() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<drogon::WebSocketConnectionPtr>> byUser_;
};

class WsController : public drogon::WebSocketController<WsController> {
public:
    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws");
    WS_PATH_LIST_END

    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& message,
                          const drogon::WebSocketMessageType& type) override;
    void handleNewConnection(const drogon::HttpRequestPtr& req,
                             const drogon::WebSocketConnectionPtr& conn) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;
};

} // namespace lumina
