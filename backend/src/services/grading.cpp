#include "services/grading.h"
#include "core/errors.h"
#include "core/i18n.h"
#include "core/util.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_map>

namespace lumina {

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string normalizeText(const std::string& in, bool caseSensitive) {
    std::string s = trimCopy(in);
    std::string out;
    out.reserve(s.size());
    bool prevSpace = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!prevSpace) out += ' ';
            prevSpace = true;
        } else {
            out += c;
            prevSpace = false;
        }
    }
    if (!caseSensitive) out = lower(out);
    return out;
}

GradeResult make(double score, const json& details) {
    GradeResult r;
    r.score = std::clamp(score, 0.0, 1.0);
    r.result = r.score >= 0.999 ? "correct" : (r.score > 0.0 ? "partial" : "wrong");
    r.details = details;
    return r;
}

std::string requireString(const json& j, const char* key, const json& answer) {
    if (!answer.is_object() || !answer.contains(key)) {
        throw unprocessable(std::string("Answer field '") + key + "' is required");
    }
    const auto& v = answer[key];
    if (!v.is_string()) throw unprocessable(std::string("Answer field '") + key + "' must be a string");
    return v.get<std::string>();
}

json localizeDeep(const json& v, const std::string& locale) {
    if (v.is_object()) {
        if (v.contains("kk") || v.contains("ru") || v.contains("en")) {
            return I18n::pick(v, locale);
        }
        json out = json::object();
        for (auto it = v.begin(); it != v.end(); ++it) out[it.key()] = localizeDeep(it.value(), locale);
        return out;
    }
    if (v.is_array()) {
        json out = json::array();
        for (const auto& item : v) out.push_back(localizeDeep(item, locale));
        return out;
    }
    return v;
}

void shuffleDeterministic(json& arr, const std::string& seed) {
    if (!arr.is_array() || arr.size() < 2) return;
    std::string key = seed;
    size_t h = std::hash<std::string>{}(key);
    for (size_t i = arr.size(); i > 1; --i) {
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdULL;
        size_t j = h % i;
        std::swap(arr[i - 1], arr[j]);
    }
}

} // namespace

GradeResult Grading::grade(const std::string& type, const json& payload,
                           const json& answerKey, const json& answer) {
    if (type == "single_choice" || type == "timed_quiz") {
        const std::string chosen = requireString({}, "option", answer);
        const auto& correct = answerKey.at("correct");
        const bool ok = correct.is_array() && !correct.empty() &&
                        correct[0].is_string() && correct[0].get<std::string>() == chosen;
        return make(ok ? 1.0 : 0.0, {{"correct_option", correct}});
    }

    if (type == "multiple_choice") {
        if (!answer.contains("options") || !answer["options"].is_array()) {
            throw unprocessable("Answer field 'options' must be an array");
        }
        std::set<std::string> selected, correct, all;
        for (const auto& o : answer["options"]) {
            if (o.is_string()) selected.insert(o.get<std::string>());
        }
        for (const auto& o : answerKey.at("correct")) correct.insert(o.get<std::string>());
        for (const auto& o : payload.value("options", json::array())) {
            if (o.contains("id")) all.insert(o["id"].get<std::string>());
        }
        int correctSelected = 0;
        for (const auto& s : selected) correctSelected += correct.count(s) ? 1 : 0;
        int wrongSelected = static_cast<int>(selected.size()) - correctSelected;
        const int totalWrong = std::max<int>(1, static_cast<int>(all.size()) - static_cast<int>(correct.size()));
        double score = static_cast<double>(correctSelected) / std::max<size_t>(1, correct.size());
        score -= static_cast<double>(wrongSelected) / totalWrong;
        score = std::clamp(score, 0.0, 1.0);
        if (selected == correct) score = 1.0;
        return make(score, {{"correct_selected", correctSelected},
                            {"wrong_selected", wrongSelected},
                            {"correct", correct}});
    }

    if (type == "true_false") {
        if (!answer.contains("value") || !answer["value"].is_boolean()) {
            throw unprocessable("Answer field 'value' must be a boolean");
        }
        const bool ok = answerKey.value("correct", false) == answer["value"].get<bool>();
        return make(ok ? 1.0 : 0.0, {{"correct", answerKey.value("correct", false)}});
    }

    if (type == "short_answer") {
        const std::string text = requireString({}, "text", answer);
        const bool caseSensitive = answerKey.value("case_sensitive", false);
        const std::string norm = normalizeText(text, caseSensitive);
        double num = 0.0;
        const bool isNum = tryParseNumber(text, num);
        for (const auto& acc : answerKey.at("accepted")) {
            if (!acc.is_string()) continue;
            const std::string a = normalizeText(acc.get<std::string>(), caseSensitive);
            if (a == norm) return make(1.0, {{"matched", acc}});
            if (isNum) {
                double target = 0.0;
                if (tryParseNumber(acc.get<std::string>(), target) &&
                    std::fabs(target - num) <= 1e-6) {
                    return make(1.0, {{"matched", acc}});
                }
            }
        }
        return make(0.0, {{"expected", answerKey.value("accepted", json::array())}});
    }

    if (type == "matching") {
        if (!answer.contains("pairs") || !answer["pairs"].is_object()) {
            throw unprocessable("Answer field 'pairs' must be an object");
        }
        const json& pairs = answerKey.at("pairs");
        int total = 0, ok = 0;
        for (auto it = pairs.begin(); it != pairs.end(); ++it) {
            ++total;
            if (answer["pairs"].contains(it.key()) && answer["pairs"][it.key()] == it.value()) ++ok;
        }
        return make(total ? static_cast<double>(ok) / total : 0.0,
                    {{"correct", ok}, {"total", total}});
    }

    if (type == "ordering") {
        if (!answer.contains("order") || !answer["order"].is_array()) {
            throw unprocessable("Answer field 'order' must be an array");
        }
        const json& expected = answerKey.at("order");
        const auto& given = answer["order"];
        if (given.size() != expected.size()) return make(0.0, {{"reason", "wrong_length"}});
        int positions = 0;
        for (size_t i = 0; i < expected.size(); ++i) {
            if (given[i] == expected[i]) ++positions;
        }
        return make(expected.empty() ? 0.0
                                     : static_cast<double>(positions) / expected.size(),
                    {{"correct_positions", positions}, {"total", expected.size()}});
    }

    if (type == "cloze") {
        if (!answer.contains("blanks") || !answer["blanks"].is_object()) {
            throw unprocessable("Answer field 'blanks' must be an object");
        }
        const json& expected = answerKey.at("blanks");
        int total = 0, ok = 0;
        for (auto it = expected.begin(); it != expected.end(); ++it) {
            ++total;
            if (answer["blanks"].contains(it.key()) && answer["blanks"][it.key()] == it.value()) ++ok;
        }
        return make(total ? static_cast<double>(ok) / total : 0.0,
                    {{"correct", ok}, {"total", total}});
    }

    if (type == "numeric") {
        double value = 0.0;
        if (answer.contains("value") && answer["value"].is_number()) {
            value = answer["value"].get<double>();
        } else if (answer.contains("text") && answer["text"].is_string()) {
            if (!tryParseNumber(answer["text"].get<std::string>(), value)) {
                throw unprocessable("Answer must be a number");
            }
        } else {
            throw unprocessable("Answer field 'value' is required");
        }
        const double target = answerKey.value("value", 0.0);
        const double tolerance = answerKey.value("tolerance", 0.01);
        const bool ok = std::fabs(value - target) <= tolerance + 1e-12;
        return make(ok ? 1.0 : 0.0, {{"value", value}, {"target", target}});
    }

    if (type == "code") {
        GradeResult r;
        r.needsSandbox = true;
        r.result = "wrong";
        return r;
    }

    throw badRequest("Unknown task type: " + type);
}

json Grading::sanitizeTask(const json& task, const std::string& locale) {
    json out;
    out["id"] = task.at("id");
    out["level_id"] = task.at("level_id");
    out["topic_id"] = task.at("topic_id");
    out["type"] = task.at("type");
    out["prompt"] = localizeDeep(task.value("prompt", json::object()), locale);
    out["difficulty"] = task.value("difficulty", 1);
    out["base_xp"] = task.value("base_xp", 30);
    out["time_limit_sec"] = task.value("time_limit_sec", 120);
    out["version"] = task.value("version", 1);

    json payload = localizeDeep(task.value("payload", json::object()), locale);
    const std::string type = task.value("type", "");
    const std::string id = task.value("id", "");

    if (type == "matching" && payload.contains("right")) {
        shuffleDeterministic(payload["right"], id + ":right");
    }
    if (type == "ordering" && payload.contains("items")) {
        shuffleDeterministic(payload["items"], id + ":items");
    }
    if (type == "code") {
        // Never expose hidden tests.
        json visible = payload.value("visible_tests", json::array());
        payload = {{"language", payload.value("language", "cpp")},
                   {"starter_code", payload.value("starter_code", "")},
                   {"visible_tests", visible}};
    }
    out["payload"] = payload;

    json hints = json::array();
    const auto& rawHints = task.value("hints", json::array());
    for (size_t i = 0; i < rawHints.size(); ++i) {
        json h;
        h["index"] = i;
        h["text"] = localizeDeep(rawHints[i], locale);
        hints.push_back(h);
    }
    out["hints"] = hints;

    // Paid hint balance is added by the caller (inventory).
    return out;
}

} // namespace lumina
