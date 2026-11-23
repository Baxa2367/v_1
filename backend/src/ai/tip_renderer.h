#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace lumina::ai {

using json = nlohmann::json;

// Locale-aware template renderer for AI tips: replaces {topic}, {streak},
// {accuracy}, {level}, {xp}, {goal} placeholders.
class TipRenderer {
public:
    static std::string render(const std::string& tmpl, const json& vars);
    static std::string fallback(const std::string& kind, const std::string& locale,
                                const json& vars);
};

} // namespace lumina::ai
