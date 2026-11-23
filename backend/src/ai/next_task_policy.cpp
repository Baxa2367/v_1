#include "ai/next_task_policy.h"
#include "ai/mastery_model.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace lumina::ai {

std::string NextTaskPolicy::reasonToString(ReasonCode r) {
    switch (r) {
        case ReasonCode::Challenge: return "challenge";
        case ReasonCode::Support: return "support";
        case ReasonCode::Review: return "review";
        case ReasonCode::NewTopic: return "new_topic";
        case ReasonCode::DailyGoal: return "daily_goal";
        default: return "zpd";
    }
}

double NextTaskPolicy::errorRate(int attempts, int errors) {
    if (attempts <= 0) return 0.0;
    return static_cast<double>(errors) / attempts;
}

Decision NextTaskPolicy::choose(const std::vector<Candidate>& all, const PolicyContext& ctx) {
    Decision d;
    std::vector<Candidate> pool;
    for (const auto& c : all) {
        if (!c.recent) pool.push_back(c);
    }
    if (pool.empty()) pool = all;
    if (pool.empty()) return d;

    std::mt19937 rng(ctx.seed ? ctx.seed : std::random_device{}());
    std::uniform_real_distribution<double> uni(0.0, 1.0);

    const double errRate = errorRate(ctx.attemptsLast10, ctx.errorsLast10);
    const bool struggling = ctx.streakWrong >= 3 || (ctx.attemptsLast10 >= 5 && errRate >= 0.6);
    const bool thriving = ctx.streakCorrect >= 3;

    auto pick = [&](const Candidate& c, ReasonCode reason) {
        Decision out;
        out.taskId = c.taskId;
        out.reason = reason;
        out.reasonCode = reasonToString(reason);
        out.topicId = c.topicId;
        out.topicTitle = c.topicTitle;
        out.expectedSuccess = MasteryModel::expectedSuccess(c.mastery, c.difficulty);
        out.features = {{"mastery", c.mastery},
                        {"confidence", c.confidence},
                        {"difficulty", c.difficulty},
                        {"days_since_seen", c.daysSinceSeen},
                        {"error_rate_10", errRate},
                        {"streak_correct", ctx.streakCorrect},
                        {"streak_wrong", ctx.streakWrong},
                        {"expected_success", out.expectedSuccess}};
        return out;
    };

    if (struggling) {
        // Drop one difficulty step inside the weakest topic and offer support.
        const Candidate* weakest = nullptr;
        for (const auto& c : pool) {
            if (!weakest || c.mastery < weakest->mastery ||
                (std::fabs(c.mastery - weakest->mastery) < 1e-9 && c.difficulty < weakest->difficulty)) {
                weakest = &c;
            }
        }
        if (weakest) {
            Candidate easy = *weakest;
            easy.difficulty = std::max(1, easiestDifficulty(pool, weakest->topicId));
            // Prefer an easier task of the same topic if one exists.
            for (const auto& c : pool) {
                if (c.topicId == weakest->topicId && c.difficulty <= easy.difficulty + 1) {
                    easy = c;
                    break;
                }
            }
            return pick(easy, ReasonCode::Support);
        }
    }

    if (thriving) {
        const Candidate* hardest = nullptr;
        for (const auto& c : pool) {
            if (!hardest || c.difficulty > hardest->difficulty) hardest = &c;
        }
        if (hardest && uni(rng) < 0.75) return pick(*hardest, ReasonCode::Challenge);
    }

    // 70% challenge zone, 20% weak topic review, 10% exploration.
    const double roll = uni(rng);
    if (roll < 0.70) {
        const Candidate* best = nullptr;
        double bestDist = 1e9;
        for (const auto& c : pool) {
            const double p = MasteryModel::expectedSuccess(c.mastery, c.difficulty);
            double dist = std::fabs(p - 0.70);
            if (p < 0.55 || p > 0.85) dist += 0.35;  // outside ZPD gets penalised
            if (dist < bestDist) {
                bestDist = dist;
                best = &c;
            }
        }
        if (best) return pick(*best, ReasonCode::Zpd);
    } else if (roll < 0.90) {
        const Candidate* weakest = nullptr;
        for (const auto& c : pool) {
            if (!weakest || c.mastery < weakest->mastery) weakest = &c;
        }
        if (weakest) return pick(*weakest, ReasonCode::Review);
    }

    // Exploration: highest difficulty candidate (new material).
    const Candidate* explore = &pool.front();
    for (const auto& c : pool) {
        if (c.mastery < 0.45 && c.difficulty > explore->difficulty) explore = &c;
    }
    return pick(*explore, ReasonCode::NewTopic);
}

int NextTaskPolicy::easiestDifficulty(const std::vector<Candidate>& pool,
                                      const std::string& topicId) {
    int best = 10;
    bool found = false;
    for (const auto& c : pool) {
        if (c.topicId == topicId) {
            best = found ? std::min(best, c.difficulty) : c.difficulty;
            found = true;
        }
    }
    return found ? best : 1;
}

} // namespace lumina::ai
