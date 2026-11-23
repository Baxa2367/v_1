#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace lumina::ai {

using json = nlohmann::json;

struct Candidate {
    std::string taskId;
    std::string topicId;
    std::string topicTitle;
    int difficulty = 1;
    double mastery = 0.3;
    double confidence = 0.1;
    double daysSinceSeen = 30.0;
    bool recent = false;          // attempted in the last N attempts
    bool generated = false;
    int baseXp = 30;
};

struct PolicyContext {
    int streakCorrect = 0;
    int streakWrong = 0;
    int attemptsLast10 = 0;
    int errorsLast10 = 0;
    bool dailyGoalMet = false;
    int dailyGoalRemainingXp = 0;
    std::string locale = "kk";
    unsigned seed = 0;
};

enum class ReasonCode { Zpd, Challenge, Support, Review, NewTopic, DailyGoal };

struct Decision {
    std::optional<std::string> taskId;
    ReasonCode reason = ReasonCode::Zpd;
    std::string reasonCode = "zpd";
    double expectedSuccess = 0.0;
    std::string topicId;
    std::string topicTitle;
    json features = json::object();
    bool empty() const { return !taskId.has_value(); }
};

class NextTaskPolicy {
public:
    static std::string reasonToString(ReasonCode r);
    static double errorRate(int attempts, int errors);

    static Decision choose(const std::vector<Candidate>& candidates, const PolicyContext& ctx);

private:
    static int easiestDifficulty(const std::vector<Candidate>& pool, const std::string& topicId);
};

} // namespace lumina::ai

