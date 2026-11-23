#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace lumina {

using json = nlohmann::json;

// Error carrying HTTP status + stable machine code + optional details.
class ApiError : public std::runtime_error {
public:
    ApiError(int status, std::string code, std::string message, json details = json::object())
        : std::runtime_error(message), status_(status), code_(std::move(code)),
          message_(std::move(message)), details_(std::move(details)) {}

    int status() const { return status_; }
    const std::string& code() const { return code_; }
    const std::string& message() const { return message_; }
    const json& details() const { return details_; }

    json toJson() const {
        json e = {{"error", {{"code", code_}, {"message", message_}}}};
        if (!details_.empty()) e["error"]["details"] = details_;
        return e;
    }

private:
    int status_;
    std::string code_;
    std::string message_;
    json details_;
};

inline ApiError badRequest(const std::string& m, json d = json::object()) {
    return {400, "bad_request", m, std::move(d)};
}
inline ApiError unauthorized(const std::string& m = "Authentication required") {
    return {401, "unauthorized", m};
}
inline ApiError forbidden(const std::string& m = "Insufficient permissions") {
    return {403, "forbidden", m};
}
inline ApiError notFound(const std::string& m = "Resource not found") {
    return {404, "not_found", m};
}
inline ApiError conflict(const std::string& m, json d = json::object()) {
    return {409, "conflict", m, std::move(d)};
}
inline ApiError unprocessable(const std::string& m, json d = json::object()) {
    return {422, "validation_error", m, std::move(d)};
}
inline ApiError tooMany(const std::string& m = "Too many requests") {
    return {429, "rate_limited", m};
}
inline ApiError internal(const std::string& m = "Internal server error") {
    return {500, "internal_error", m};
}

} // namespace lumina
