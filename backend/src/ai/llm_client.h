#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace lumina::ai {

using json = nlohmann::json;

// Optional Claude integration. When ANTHROPIC_API_KEY is absent every method
// returns std::nullopt and callers fall back to the task bank / templates.
class LlmClient {
public:
    static bool available();
    static std::optional<json> generateTask(const std::string& topic, int difficulty,
                                            const std::string& locale);
    static std::optional<std::string> personalizeTip(const std::string& baseTip,
                                                     const json& context,
                                                     const std::string& locale);
    static std::optional<std::string> explainChoice(const json& features,
                                                    const std::string& locale);

private:
    static std::optional<std::string> complete(const std::string& system,
                                               const std::string& user, int maxTokens = 900);
};

} // namespace lumina::ai
