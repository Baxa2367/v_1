#include "ai/mastery_model.h"
#include "ai/next_task_policy.h"
#include <gtest/gtest.h>

using namespace lumina::ai;

TEST(Mastery, CorrectAnswerIncreasesMastery) {
    MasteryState s;
    s.mastery = 0.4;
    auto up = MasteryModel::update(s, true, 4);
    auto down = MasteryModel::update(s, false, 4);
    EXPECT_GT(up.mastery, s.mastery);
    EXPECT_LT(down.mastery, s.mastery);
    EXPECT_EQ(up.streakCorrect, 1);
    EXPECT_EQ(down.streakWrong, 1);
}

TEST(Mastery, ConfidenceGrows) {
    MasteryState s;
    auto next = MasteryModel::update(s, true, 3);
    EXPECT_GT(next.confidence, s.confidence);
}

TEST(Mastery, DecayPullsTowardPrior) {
    EXPECT_NEAR(MasteryModel::decay(0.9, 21), 0.6, 0.01);
    EXPECT_NEAR(MasteryModel::decay(0.3, 10), 0.3, 0.001);
    EXPECT_GT(MasteryModel::decay(0.9, 0), 0.89);
}

TEST(Mastery, ExpectedSuccessDecreasesWithDifficulty) {
    EXPECT_GT(MasteryModel::expectedSuccess(0.8, 2), 0.8);
    EXPECT_LT(MasteryModel::expectedSuccess(0.8, 10), 0.8);
}

TEST(Policy, StrugglingStudentGetsEasierTask) {
    std::vector<Candidate> pool;
    for (int d = 1; d <= 10; ++d) {
        Candidate c;
        c.taskId = "t" + std::to_string(d);
        c.topicId = "tp";
        c.topicTitle = "Loops";
        c.difficulty = d;
        c.mastery = 0.3;
        pool.push_back(c);
    }
    PolicyContext ctx;
    ctx.streakWrong = 4;
    ctx.attemptsLast10 = 8;
    ctx.errorsLast10 = 6;
    auto decision = NextTaskPolicy::choose(pool, ctx);
    ASSERT_TRUE(decision.taskId.has_value());
    EXPECT_EQ(decision.reasonCode, "support");
    EXPECT_LE(decision.features["difficulty"].get<int>(), 3);
}

TEST(Policy, ThrivingStudentGetsHarderTask) {
    std::vector<Candidate> pool;
    for (int d = 1; d <= 10; ++d) {
        Candidate c;
        c.taskId = "t" + std::to_string(d);
        c.topicId = "tp";
        c.difficulty = d;
        c.mastery = 0.8;
        pool.push_back(c);
    }
    PolicyContext ctx;
    ctx.streakCorrect = 5;
    auto decision = NextTaskPolicy::choose(pool, ctx);
    ASSERT_TRUE(decision.taskId.has_value());
    EXPECT_GE(decision.features["difficulty"].get<int>(), 7);
}

TEST(Policy, AvoidsRecentTasks) {
    std::vector<Candidate> pool;
    for (int i = 0; i < 6; ++i) {
        Candidate c;
        c.taskId = "t" + std::to_string(i);
        c.difficulty = 1 + i;
        c.mastery = 0.5;
        c.recent = i < 5;
        pool.push_back(c);
    }
    PolicyContext ctx;
    auto decision = NextTaskPolicy::choose(pool, ctx);
    ASSERT_TRUE(decision.taskId.has_value());
    EXPECT_EQ(*decision.taskId, "t5");
}

TEST(Policy, EmptyPoolReturnsEmptyDecision) {
    auto decision = NextTaskPolicy::choose({}, PolicyContext{});
    EXPECT_TRUE(decision.empty());
}
