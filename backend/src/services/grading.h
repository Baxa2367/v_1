#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace lumina {

using json = nlohmann::json;

struct GradeResult {
    double score = 0.0;          // 0..1
    std::string result;          // correct | partial | wrong
    json details = json::object();
    bool needsSandbox = false;   // code tasks are checked by the sandbox service
};

class Grading {
public:
    // Pure grading for all non-code types. Throws ApiError on malformed answers.
    static GradeResult grade(const std::string& type, const json& payload,
                             const json& answerKey, const json& answer);

    static json sanitizeTask(const json& taskRow, const std::string& locale);
};

} // namespace lumina
