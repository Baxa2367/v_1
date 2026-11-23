#include "ai/mastery_model.h"
#include <algorithm>
#include <cmath>

namespace lumina::ai {

MasteryState MasteryModel::update(const MasteryState& s, bool correct, int difficulty) {
    MasteryState out = s;
    out.attempts += 1;

    const double m = std::clamp(s.mastery, 0.01, 0.99);
    const double slip = 0.10;   // knows but answers wrong
    const double guess = 0.22;  // does not know but guesses right
    // Harder tasks slightly increase slip, easier decrease it.
    const double dAdj = std::clamp((difficulty - 5) * 0.01, -0.04, 0.06);
    const double slipAdj = std::clamp(slip + dAdj, 0.05, 0.2);

    double posterior;
    if (correct) {
        posterior = (m * (1.0 - slipAdj)) / (m * (1.0 - slipAdj) + (1.0 - m) * guess);
        out.correct += 1;
        out.streakCorrect += 1;
        out.streakWrong = 0;
    } else {
        posterior = (m * slipAdj) / (m * slipAdj + (1.0 - m) * (1.0 - guess));
        out.streakWrong += 1;
        out.streakCorrect = 0;
    }

    const double learn = correct ? 0.16 : 0.06;
    double next = posterior + (1.0 - posterior) * learn;
    // Wrong answers on easy tasks should hurt a bit more.
    if (!correct && difficulty <= 3) next -= 0.03;
    out.mastery = std::clamp(next, 0.02, 0.99);
    out.confidence = std::clamp(s.confidence + 0.09 * (1.0 - s.confidence), 0.05, 0.99);
    return out;
}

double MasteryModel::decay(double mastery, double days) {
    const double prior = 0.30;
    const double halfLife = 21.0;
    const double factor = std::pow(0.5, std::max(0.0, days) / halfLife);
    return std::clamp(prior + (mastery - prior) * factor, 0.02, 0.99);
}

double MasteryModel::expectedSuccess(double mastery, int difficulty) {
    const double x = 0.62 * (difficulty - 10.0 * mastery);
    return 1.0 / (1.0 + std::exp(x));
}

} // namespace lumina::ai
