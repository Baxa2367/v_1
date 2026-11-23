#include "services/xp_engine.h"
#include <algorithm>
#include <cmath>

namespace lumina {

json XpBreakdown::toJson() const {
    return {
        {"amount", amount},
        {"difficulty_multiplier", difficultyMultiplier},
        {"accuracy_factor", accuracyFactor},
        {"speed_factor", speedFactor},
        {"streak_multiplier", streakMultiplier},
        {"hint_penalty", hintPenalty},
        {"repeat_decay", repeatDecay},
    };
}

XpBreakdown XpEngine::compute(const XpInput& in) {
    XpBreakdown out;
    if (in.suspicious || in.score <= 0.0) {
        out.amount = 0;
        return out;
    }

    const int difficulty = std::clamp(in.difficulty, 1, 10);
    out.difficultyMultiplier = 0.8 + 0.12 * difficulty;
    out.accuracyFactor = 0.4 + 0.6 * std::clamp(in.score, 0.0, 1.0);
    out.hintPenalty = std::min(0.45, 0.15 * std::max(0, in.hintCount));
    out.repeatDecay = in.repeated ? 0.5 : 1.0;

    const double limitMs = std::max(1.0, in.timeLimitSec * 1000.0);
    const double ratio = std::clamp(static_cast<double>(in.timeSpentMs) / limitMs, 0.0, 1.0);
    // ratio 0.3 -> 1.15, ratio 1.0 -> 0.85
    out.speedFactor = 1.15 - 0.30 * std::clamp((ratio - 0.3) / 0.7, 0.0, 1.0);

    const int streak = std::clamp(in.streak, 0, 10);
    out.streakMultiplier = 1.0 + 0.05 * streak;

    double xp = static_cast<double>(in.baseXp) * out.difficultyMultiplier *
                out.accuracyFactor * out.speedFactor * out.streakMultiplier *
                (1.0 - out.hintPenalty) * out.repeatDecay;
    out.amount = std::max(1, static_cast<int>(std::lround(xp)));
    return out;
}

int XpEngine::xpToNext(int level) {
    if (level < 1) level = 1;
    return static_cast<int>(std::lround(100.0 * std::pow(level, 1.5)));
}

int XpEngine::xpToReachLevel(int level) {
    int total = 0;
    for (int l = 1; l < level; ++l) total += xpToNext(l);
    return total;
}

int XpEngine::levelForXp(long long totalXp) {
    int level = 1;
    int remaining = static_cast<int>(std::min<long long>(totalXp, 1'000'000'000LL));
    while (remaining >= xpToNext(level) && level < 100) {
        remaining -= xpToNext(level);
        ++level;
    }
    return level;
}

int XpEngine::titleIndexForLevel(int level) {
    if (level >= 20) return 7;
    if (level >= 16) return 6;
    if (level >= 13) return 5;
    if (level >= 10) return 4;
    if (level >= 7) return 3;
    if (level >= 5) return 2;
    if (level >= 3) return 1;
    return 0;
}

std::string XpEngine::titleForLevel(int level, const std::string& locale) {
    static const char* titles[8][3] = {
        {"Жаңадан келген", "Новичок", "Novice"},
        {"Оқушы", "Ученик", "Apprentice"},
        {"Зерттеуші", "Исследователь", "Explorer"},
        {"Шебер", "Мастер", "Adept"},
        {"Сарапшы", "Эксперт", "Expert"},
        {"Жүйе сәулетшісі", "Системный архитектор", "Systems Architect"},
        {"Аңыз", "Легенда", "Legend"},
        {"Миф", "Миф", "Mythic"},
    };
    const int idx = titleIndexForLevel(level);
    if (locale == "ru") return titles[idx][1];
    if (locale == "en") return titles[idx][2];
    return titles[idx][0];
}

} // namespace lumina
