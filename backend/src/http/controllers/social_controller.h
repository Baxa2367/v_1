#pragma once
#include "http/http_utils.h"
#include <drogon/HttpController.h>

namespace lumina {

using http::Callback;
using http::HttpRequestPtr;

class SocialController : public drogon::HttpController<SocialController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SocialController::leaderboard, "/api/v1/leaderboards", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::leaderboardMe, "/api/v1/leaderboards/me", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::shop, "/api/v1/shop", drogon::Get, "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::buy, "/api/v1/shop/{id}/buy", drogon::Post,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::inventory, "/api/v1/inventory", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::equip, "/api/v1/inventory/{id}/equip", drogon::Post,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::seasons, "/api/v1/seasons", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::currentSeason, "/api/v1/seasons/current", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::seasonRewards, "/api/v1/seasons/{id}/rewards", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::claimSeason, "/api/v1/seasons/{id}/claim/{tier}", drogon::Post,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::teams, "/api/v1/teams", drogon::Get, "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::myTeam, "/api/v1/teams/mine", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::createTeam, "/api/v1/teams", drogon::Post,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::joinTeam, "/api/v1/teams/join", drogon::Post,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::leaveTeam, "/api/v1/teams/leave", drogon::Post,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::team, "/api/v1/teams/{id}", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::tournaments, "/api/v1/tournaments", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::standings, "/api/v1/tournaments/{id}/standings", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::reportProgress, "/api/v1/reports/progress", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::reportXp, "/api/v1/reports/xp", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::reportTopics, "/api/v1/reports/topics", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::reportAchievements, "/api/v1/reports/achievements", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::reportLeaderboard, "/api/v1/reports/leaderboard", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::reportHeatmap, "/api/v1/reports/heatmap", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::reportOverview, "/api/v1/reports/overview", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::exportReport, "/api/v1/reports/export", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::myXp, "/api/v1/me/xp", drogon::Get, "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::myXpEvents, "/api/v1/me/xp/events", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::myAchievements, "/api/v1/me/achievements", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::myStreak, "/api/v1/me/streak", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::myQuests, "/api/v1/me/quests", drogon::Get,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::claimQuest, "/api/v1/quests/{id}/claim", drogon::Post,
                  "lumina::AuthFilter");
    ADD_METHOD_TO(SocialController::myHeatmap, "/api/v1/me/heatmap", drogon::Get,
                  "lumina::AuthFilter");
    METHOD_LIST_END

    void leaderboard(const HttpRequestPtr& req, Callback&& cb);
    void leaderboardMe(const HttpRequestPtr& req, Callback&& cb);
    void shop(const HttpRequestPtr& req, Callback&& cb);
    void buy(const HttpRequestPtr& req, Callback&& cb);
    void inventory(const HttpRequestPtr& req, Callback&& cb);
    void equip(const HttpRequestPtr& req, Callback&& cb);
    void seasons(const HttpRequestPtr& req, Callback&& cb);
    void currentSeason(const HttpRequestPtr& req, Callback&& cb);
    void seasonRewards(const HttpRequestPtr& req, Callback&& cb);
    void claimSeason(const HttpRequestPtr& req, Callback&& cb);
    void teams(const HttpRequestPtr& req, Callback&& cb);
    void myTeam(const HttpRequestPtr& req, Callback&& cb);
    void createTeam(const HttpRequestPtr& req, Callback&& cb);
    void joinTeam(const HttpRequestPtr& req, Callback&& cb);
    void leaveTeam(const HttpRequestPtr& req, Callback&& cb);
    void team(const HttpRequestPtr& req, Callback&& cb);
    void tournaments(const HttpRequestPtr& req, Callback&& cb);
    void standings(const HttpRequestPtr& req, Callback&& cb);
    void reportProgress(const HttpRequestPtr& req, Callback&& cb);
    void reportXp(const HttpRequestPtr& req, Callback&& cb);
    void reportTopics(const HttpRequestPtr& req, Callback&& cb);
    void reportAchievements(const HttpRequestPtr& req, Callback&& cb);
    void reportLeaderboard(const HttpRequestPtr& req, Callback&& cb);
    void reportHeatmap(const HttpRequestPtr& req, Callback&& cb);
    void reportOverview(const HttpRequestPtr& req, Callback&& cb);
    void exportReport(const HttpRequestPtr& req, Callback&& cb);
    void myXp(const HttpRequestPtr& req, Callback&& cb);
    void myXpEvents(const HttpRequestPtr& req, Callback&& cb);
    void myAchievements(const HttpRequestPtr& req, Callback&& cb);
    void myStreak(const HttpRequestPtr& req, Callback&& cb);
    void myQuests(const HttpRequestPtr& req, Callback&& cb);
    void claimQuest(const HttpRequestPtr& req, Callback&& cb);
    void myHeatmap(const HttpRequestPtr& req, Callback&& cb);
};

} // namespace lumina

