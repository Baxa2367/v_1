#include "services/grading.h"
#include <gtest/gtest.h>

using namespace lumina;

namespace {
json options() {
    return json::array({
        json{{"id", "a"}, {"text", "A"}},
        json{{"id", "b"}, {"text", "B"}},
        json{{"id", "c"}, {"text", "C"}},
        json{{"id", "d"}, {"text", "D"}},
    });
}
} // namespace

TEST(Grading, SingleChoice) {
    json payload = {{"options", options()}};
    json key = {{"correct", {"b"}}};
    auto ok = Grading::grade("single_choice", payload, key, {{"option", "b"}});
    EXPECT_EQ(ok.result, "correct");
    EXPECT_DOUBLE_EQ(ok.score, 1.0);
    auto bad = Grading::grade("single_choice", payload, key, {{"option", "a"}});
    EXPECT_EQ(bad.result, "wrong");
}

TEST(Grading, MultipleChoicePartialCredit) {
    json payload = {{"options", options()}};
    json key = {{"correct", {"a", "c"}}};
    auto perfect = Grading::grade("multiple_choice", payload, key, {{"options", {"a", "c"}}});
    EXPECT_DOUBLE_EQ(perfect.score, 1.0);
    auto half = Grading::grade("multiple_choice", payload, key, {{"options", {"a"}}});
    EXPECT_NEAR(half.score, 0.5, 0.001);
    auto cluttered = Grading::grade("multiple_choice", payload, key, {{"options", {"a", "b"}}});
    EXPECT_LT(cluttered.score, 1.0);
    EXPECT_GT(cluttered.score, 0.0);
}

TEST(Grading, TrueFalse) {
    auto ok = Grading::grade("true_false", json::object(), {{"correct", true}}, {{"value", true}});
    EXPECT_EQ(ok.score, 1.0);
    auto bad = Grading::grade("true_false", json::object(), {{"correct", true}}, {{"value", false}});
    EXPECT_EQ(bad.score, 0.0);
}

TEST(Grading, ShortAnswerNormalisation) {
    json key = {{"accepted", {"std::vector"}}, {"case_sensitive", false}};
    auto ok = Grading::grade("short_answer", json::object(), key, {{"text", "  STD::VECTOR "}});
    EXPECT_EQ(ok.score, 1.0);
    auto bad = Grading::grade("short_answer", json::object(), key, {{"text", "vector"}});
    EXPECT_EQ(bad.score, 0.0);
}

TEST(Grading, NumericTolerance) {
    json key = {{"value", 3.14159}, {"tolerance", 0.01}};
    EXPECT_EQ(Grading::grade("numeric", json::object(), key, {{"text", "3,14"}}).score, 1.0);
    EXPECT_EQ(Grading::grade("numeric", json::object(), key, {{"value", 3.2}}).score, 0.0);
}

TEST(Grading, MatchingPartial) {
    json payload = {{"left", json::array({{{"id", "l1"}}, {{"id", "l2"}}})},
                    {"right", json::array({{{"id", "r1"}}, {{"id", "r2"}}})}};
    json key = {{"pairs", {{"l1", "r1"}, {"l2", "r2"}}}};
    auto half = Grading::grade("matching", payload, key, {{"pairs", {{"l1", "r1"}, {"l2", "r1"}}}});
    EXPECT_NEAR(half.score, 0.5, 0.001);
    auto all = Grading::grade("matching", payload, key, {{"pairs", {{"l1", "r1"}, {"l2", "r2"}}}});
    EXPECT_DOUBLE_EQ(all.score, 1.0);
}

TEST(Grading, Ordering) {
    json payload = {{"items", json::array({{{"id", "x"}}, {{"id", "y"}}, {{"id", "z"}}})}};
    json key = {{"order", {"z", "x", "y"}}};
    EXPECT_DOUBLE_EQ(Grading::grade("ordering", payload, key, {{"order", {"z", "x", "y"}}}).score, 1.0);
    EXPECT_NEAR(Grading::grade("ordering", payload, key, {{"order", {"x", "z", "y"}}}).score, 1.0 / 3, 0.001);
}

TEST(Grading, Cloze) {
    json payload = {{"text", "a {{1}} b {{2}}"}};
    json key = {{"blanks", {{"1", "o1"}, {"2", "o2"}}}};
    EXPECT_DOUBLE_EQ(Grading::grade("cloze", payload, key, {{"blanks", {{"1", "o1"}, {"2", "o2"}}}}).score, 1.0);
    EXPECT_NEAR(Grading::grade("cloze", payload, key, {{"blanks", {{"1", "o1"}, {"2", "oX"}}}}).score, 0.5, 0.001);
}

TEST(Grading, SanitizeHidesAnswerKey) {
    json task = {{"id", "t1"}, {"level_id", "l1"}, {"topic_id", "tp1"}, {"type", "single_choice"},
                 {"prompt", {{"kk", "РЎТ±СЂР°Т›"}, {"ru", "Р’РѕРїСЂРѕСЃ"}}},
                 {"payload", {{"options", options()}}},
                 {"answer_key", {{"correct", {"b"}}}},
                 {"hints", json::array()}, {"difficulty", 3}, {"base_xp", 40},
                 {"time_limit_sec", 60}};
    auto sanitized = Grading::sanitizeTask(task, "ru");
    EXPECT_FALSE(sanitized.contains("answer_key"));
    EXPECT_EQ(sanitized["prompt"], "Р’РѕРїСЂРѕСЃ");
    EXPECT_EQ(sanitized["payload"]["options"].size(), 4u);
}

