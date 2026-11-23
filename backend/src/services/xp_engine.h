#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace lumina {

using json = nlohmann::json;

struct XpInput {
    int baseXp = 30;
    int difficulty = 1;
    double score = 1.0;      // 0..1
    int timeSpentMs = 0;
    int timeLimitSec = 120;
    int hintCount = 0;
    int streak = 0;          // current correct streak before this attempt
    bool repeated = false;   // task already attempted before
    bool suspicious = false;
};

struct XpBreakdown {
    int amount = 0;
    double difficultyMultiplier = 1.0;
    double accuracyFactor = 1.0;
    double speedFactor = 1.0;
    double streakMultiplier = 1.0;
    double hintPenalty = 0.0;
    double repeatDecay = 1.0;
    json toJson() const;
};

class XpEngine {
public:
    static XpBreakdown compute(const XpInput& in);

    // xp_to_next(level) = round(100 * level^1.5)
    static int xpToNext(int level);
    static int xpToReachLevel(int level);   // cumulative XP needed to reach `level`
    static int levelForXp(long long totalXp);
    static std::string titleForLevel(int level, const std::string& locale);
    static int titleIndexForLevel(int level);
};

} // namespace lumina
