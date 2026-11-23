#pragma once
#include <nlohmann/json.hpp>

namespace lumina::ai {

using json = nlohmann::json;

struct MasteryState {
    double mastery = 0.30;   // P(knows topic)
    double confidence = 0.10;
    int attempts = 0;
    int correct = 0;
    int streakCorrect = 0;
    int streakWrong = 0;
};

// Simplified Bayesian Knowledge Tracing with Elo-flavoured difficulty.
class MasteryModel {
public:
    static MasteryState update(const MasteryState& state, bool correct, int difficulty);

    // Forgetting curve: pulls mastery toward the 0.3 prior, half-life ~21 days.
    static double decay(double mastery, double days);

    // Probability of a correct answer for a student with `mastery` on a task
    // of the given difficulty (logistic, calibrated so mastery 0.7 & d=5 ~ 0.7).
    static double expectedSuccess(double mastery, int difficulty);
};

} // namespace lumina::ai
