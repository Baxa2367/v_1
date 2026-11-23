#include "services/xp_engine.h"
#include <gtest/gtest.h>

using namespace lumina;

TEST(XpEngine, DifficultyAndAccuracyIncreaseXp) {
    XpInput low;
    low.baseXp = 50;
    low.difficulty = 2;
    low.score = 1.0;
    low.timeSpentMs = 30000;
    low.timeLimitSec = 120;

    XpInput high = low;
    high.difficulty = 9;

    EXPECT_GT(XpEngine::compute(high).amount, XpEngine::compute(low).amount);

    XpInput partial = low;
    partial.score = 0.5;
    EXPECT_LT(XpEngine::compute(partial).amount, XpEngine::compute(low).amount);
}

TEST(XpEngine, WrongAnswerGivesNoXp) {
    XpInput in;
    in.score = 0.0;
    EXPECT_EQ(XpEngine::compute(in).amount, 0);
}

TEST(XpEngine, SuspiciousAttemptGivesNoXp) {
    XpInput in;
    in.suspicious = true;
    EXPECT_EQ(XpEngine::compute(in).amount, 0);
}

TEST(XpEngine, HintsReduceXp) {
    XpInput base;
    base.baseXp = 100;
    base.score = 1.0;
    base.timeSpentMs = 10000;
    base.timeLimitSec = 120;

    XpInput hinted = base;
    hinted.hintCount = 2;
    EXPECT_LT(XpEngine::compute(hinted).amount, XpEngine::compute(base).amount);
}

TEST(XpEngine, RepeatHalvesXp) {
    XpInput base;
    base.baseXp = 100;
    base.score = 1.0;
    XpInput repeated = base;
    repeated.repeated = true;
    EXPECT_LT(XpEngine::compute(repeated).amount, XpEngine::compute(base).amount);
    EXPECT_DOUBLE_EQ(XpEngine::compute(repeated).repeatDecay, 0.5);
}

TEST(XpEngine, FastAnswersGetBonus) {
    XpInput fast;
    fast.baseXp = 80;
    fast.score = 1.0;
    fast.timeSpentMs = 2000;
    fast.timeLimitSec = 120;
    XpInput slow = fast;
    slow.timeSpentMs = 119000;
    EXPECT_GT(XpEngine::compute(fast).amount, XpEngine::compute(slow).amount);
}

TEST(XpEngine, LevelCurve) {
    EXPECT_EQ(XpEngine::xpToNext(1), 100);
    EXPECT_EQ(XpEngine::xpToNext(2), 283);
    EXPECT_EQ(XpEngine::xpToReachLevel(1), 0);
    EXPECT_EQ(XpEngine::xpToReachLevel(2), 100);
    EXPECT_EQ(XpEngine::levelForXp(0), 1);
    EXPECT_EQ(XpEngine::levelForXp(99), 1);
    EXPECT_EQ(XpEngine::levelForXp(100), 2);
    EXPECT_EQ(XpEngine::levelForXp(382), 2);
    EXPECT_EQ(XpEngine::levelForXp(383), 3);
}

TEST(XpEngine, TitlesAreLocalised) {
    EXPECT_EQ(XpEngine::titleForLevel(1, "kk"), "Жаңадан келген");
    EXPECT_EQ(XpEngine::titleForLevel(1, "ru"), "Новичок");
    EXPECT_EQ(XpEngine::titleForLevel(1, "en"), "Novice");
    EXPECT_NE(XpEngine::titleForLevel(14, "kk"), XpEngine::titleForLevel(14, "ru"));
}
