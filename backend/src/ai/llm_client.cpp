#include "ai/llm_client.h"
#include "core/config.h"
#include "core/log.h"
#include "core/util.h"

#if defined(LUMINA_HAVE_CURL)
#include <curl/curl.h>
#endif

namespace lumina::ai {

namespace {

#if defined(LUMINA_HAVE_CURL)
size_t writeCb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}
#endif

} // namespace

bool LlmClient::available() {
    return !Config::instance().get("ANTHROPIC_API_KEY").empty();
}

std::optional<std::string> LlmClient::complete(const std::string& system,
                                               const std::string& user, int maxTokens) {
#if defined(LUMINA_HAVE_CURL)
    const std::string key = Config::instance().get("ANTHROPIC_API_KEY");
    if (key.empty()) return std::nullopt;
    const std::string model = Config::instance().get("ANTHROPIC_MODEL", "claude-sonnet-4-20250514");

    json body = {
        {"model", model},
        {"max_tokens", maxTokens},
        {"system", system},
        {"messages", json::array({{{"role", "user"}, {"content", user}}})},
    };

    CURL* curl = curl_easy_init();
    if (!curl) return std::nullopt;
    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("x-api-key: " + key).c_str());
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    headers = curl_slist_append(headers, "content-type: application/json");
    const std::string payload = body.dump();
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.anthropic.com/v1/messages");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    const CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK || status != 200) {
        Log::get()->warn("anthropic request failed: rc={} status={}", (int)rc, status);
        return std::nullopt;
    }
    try {
        auto j = json::parse(response);
        if (j.contains("content") && j["content"].is_array() && !j["content"].empty()) {
            return j["content"][0].value("text", "");
        }
    } catch (const std::exception& e) {
        Log::get()->warn("anthropic response parse error: {}", e.what());
    }
#else
    (void)system;
    (void)user;
    (void)maxTokens;
#endif
    return std::nullopt;
}

std::optional<json> LlmClient::generateTask(const std::string& topic, int difficulty,
                                            const std::string& locale) {
    const std::string system =
        "You are a curriculum author. Reply with strict JSON only, no markdown.";
    const std::string user =
        "Create one multiple-choice task about '" + topic + "' with difficulty " +
        std::to_string(difficulty) + "/10. Language: " + locale +
        ". JSON schema: {\"prompt\":string,\"options\":[{\"id\":\"a\",\"text\":string}],"
        "\"correct\":\"a\",\"explanation\":string,\"hints\":[string,string]}. "
        "Make exactly one option correct and return 4 options.";
    auto text = complete(system, user, 700);
    if (!text) return std::nullopt;
    try {
        auto j = json::parse(*text);
        if (!j.contains("prompt") || !j.contains("options") || !j.contains("correct")) {
            return std::nullopt;
        }
        return j;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> LlmClient::personalizeTip(const std::string& baseTip,
                                                     const json& context,
                                                     const std::string& locale) {
    const std::string system = "You are a supportive tutor. Reply with 1-2 sentences, no markdown.";
    const std::string user = "Rewrite this advice in " + locale +
                             " for a student with context " + context.dump() +
                             ". Keep it concrete and kind. Advice: " + baseTip;
    return complete(system, user, 250);
}

std::optional<std::string> LlmClient::explainChoice(const json& features,
                                                    const std::string& locale) {
    const std::string system = "You explain adaptive learning decisions. Reply with 1 sentence.";
    const std::string user = "In " + locale + ", explain why this task was selected: " +
                             features.dump();
    return complete(system, user, 200);
}

} // namespace lumina::ai
