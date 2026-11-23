#include "services/season_service.h"
#include "core/i18n.h"
#include "core/log.h"
#include "core/util.h"
#include "services/leaderboard_service.h"
#include "services/xp_engine.h"

#include <algorithm>

namespace lumina {

namespace {

json localizeSeason(const json& row, const std::string& locale) {
    json out = row;
    out["title"] = I18n::pick(row["title"], locale);
    if (row.contains("description")) out["description"] = I18n::pick(row["description"], locale);
    return out;
}

json localizeReward(const json& row, const std::string& locale) {
    json out = row;
    out["title"] = I18n::pick(row["title"], locale);
    if (row.contains("description")) out["description"] = I18n::pick(row["description"], locale);
    return out;
}

} // namespace

void SeasonService::refreshTier(Tx& t, const std::string& seasonId, const std::string& studentId) {
    const int tier = static_cast<int>(t.scalarInt(
        "SELECT count(*) FROM season_rewards WHERE season_id = $1 AND required_xp <= "
        "coalesce((SELECT season_xp FROM season_progress WHERE season_id = $1 AND student_id = $2), 0)",
        {param(seasonId), param(studentId)}));
    t.exec("UPDATE season_progress SET tier = $3, updated_at = now() "
           "WHERE season_id = $1 AND student_id = $2",
           {param(seasonId), param(studentId), param(tier)});
}

json SeasonService::current(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        auto season = t.one(
            "SELECT * FROM seasons WHERE status = 'active' AND now() BETWEEN starts_at AND ends_at "
            "ORDER BY starts_at DESC LIMIT 1");
        if (season.is_null()) {
            auto upcoming = t.one("SELECT * FROM seasons WHERE status = 'upcoming' "
                                  "ORDER BY starts_at LIMIT 1");
            return json{{"season", upcoming.is_null() ? json(nullptr)
                                                       : localizeSeason(upcoming, ctx.locale)},
                        {"active", false}};
        }
        const std::string seasonId = season["id"].get<std::string>();
        json out = localizeSeason(season, ctx.locale);
        out["active"] = true;
        out["days_left"] = t.scalarInt(
            "SELECT greatest(0, extract(day FROM ends_at - now())::int)", {param(seasonId)});

        auto rewards = t.query("SELECT * FROM season_rewards WHERE season_id = $1 ORDER BY tier",
                               {param(seasonId)});
        json rewardArr = json::array();
        for (const auto& r : rewards) rewardArr.push_back(localizeReward(r, ctx.locale));

        auto progress = t.one(
            "SELECT season_xp, tier, claimed_tiers FROM season_progress "
            "WHERE season_id = $1 AND student_id = $2",
            {param(seasonId), param(ctx.userId)});

        auto top = t.query(
            "SELECT sp.student_id, s.nickname, s.avatar, s.level, sp.season_xp AS score, "
            "row_number() OVER (ORDER BY sp.season_xp DESC) AS rank "
            "FROM season_progress sp JOIN students s ON s.user_id = sp.student_id "
            "WHERE sp.season_id = $1 ORDER BY sp.season_xp DESC LIMIT 10",
            {param(seasonId)});

        int myRank = 0;
        const long long myXp = progress.is_null() ? 0 : progress.value("season_xp", 0LL);
        if (!progress.is_null()) {
            myRank = static_cast<int>(t.scalarInt(
                          "SELECT count(*) + 1 FROM season_progress WHERE season_id = $1 "
                          "AND season_xp > $2",
                          {param(seasonId), param(myXp)}));
        }
        return json{{"season", out},
                    {"active", true},
                    {"rewards", rewardArr},
                    {"progress", progress.is_null()
                                     ? json{{"season_xp", 0}, {"tier", 0}, {"claimed_tiers", json::array()}}
                                     : progress},
                    {"rank", myRank},
                    {"top", top}};
    });
}

json SeasonService::list(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        auto rows = t.query("SELECT * FROM seasons ORDER BY starts_at DESC");
        json out = json::array();
        for (const auto& r : rows) out.push_back(localizeSeason(r, ctx.locale));
        return out;
    });
}

json SeasonService::rewards(const AuthCtx& ctx, const std::string& seasonId) {
    return Db::instance().tx([&](Tx& t) {
        auto rows = t.query("SELECT * FROM season_rewards WHERE season_id = $1 ORDER BY tier",
                            {param(seasonId)});
        json out = json::array();
        for (const auto& r : rows) out.push_back(localizeReward(r, ctx.locale));
        return out;
    });
}

json SeasonService::claim(const AuthCtx& ctx, const std::string& seasonId, int tier) {
    return Db::instance().tx([&](Tx& t) {
        auto progress = t.one(
            "SELECT season_xp, tier, claimed_tiers FROM season_progress "
            "WHERE season_id = $1 AND student_id = $2 FOR UPDATE",
            {param(seasonId), param(ctx.userId)});
        if (progress.is_null()) throw badRequest("No season progress yet");

        auto reward = t.one("SELECT * FROM season_rewards WHERE season_id = $1 AND tier = $2",
                            {param(seasonId), param(tier)});
        if (reward.is_null()) throw notFound("Reward tier not found");
        if (progress.value("tier", 0) < tier) throw badRequest("Tier is not unlocked yet");

        json claimed = progress.value("claimed_tiers", json::array());
        for (const auto& c : claimed) {
            if (c.is_number_integer() && c.get<int>() == tier) {
                throw conflict("Reward already claimed");
            }
        }
        claimed.push_back(tier);
        t.exec("UPDATE season_progress SET claimed_tiers = $3::jsonb, updated_at = now() "
               "WHERE season_id = $1 AND student_id = $2",
               {param(seasonId), param(ctx.userId), paramJson(claimed)});

        const json payload = reward.value("payload", json::object());
        const int coins = payload.value("coins", 0);
        const int xp = payload.value("xp", 0);
        json granted = json::object();
        if (coins > 0) {
            t.exec("UPDATE students SET coins = coins + $1, updated_at = now() WHERE user_id = $2",
                   {param(coins), param(ctx.userId)});
            granted["coins"] = coins;
        }
        if (xp > 0) {
            auto balance = t.scalarInt("SELECT total_xp FROM students WHERE user_id = $1",
                                       {param(ctx.userId)});
            svc::addXp(t, ctx.userId, xp, "season_reward", "season", seasonId, "", seasonId,
                       balance);
            t.exec("UPDATE students SET total_xp = total_xp + $1 WHERE user_id = $2",
                   {param(xp), param(ctx.userId)});
            granted["xp"] = xp;
            t.exec("UPDATE season_progress SET season_xp = season_xp + $1 "
                   "WHERE season_id = $2 AND student_id = $3",
                   {param(xp), param(seasonId), param(ctx.userId)});
        }
        if (payload.contains("item_code")) {
            const std::string code = payload["item_code"].get<std::string>();
            auto item = t.one("SELECT id FROM shop_items WHERE code = $1", {param(code)});
            if (!item.is_null()) {
                t.exec("INSERT INTO inventory (student_id, item_id, qty) VALUES ($1, $2, 1) "
                       "ON CONFLICT (student_id, item_id) DO UPDATE SET qty = inventory.qty + 1",
                       {param(ctx.userId), param(item["id"].get<std::string>())});
                granted["item_code"] = code;
            }
        }
        svc::notify(t, ctx.userId, "season_reward",
                    json{{"kk", "Маусым сыйлығы алынды"}, {"ru", "Награда сезона получена"},
                         {"en", "Season reward claimed"}},
                    json{{"kk", "Деңгей " + std::to_string(tier)},
                         {"ru", "Тир " + std::to_string(tier)}},
                    {{"season_id", seasonId}, {"tier", tier}});
        return json{{"claimed", true}, {"tier", tier}, {"granted", granted}};
    });
}

void SeasonService::closeSeason(const std::string& seasonId) {
    Db::instance().tx([&](Tx& t) {
        auto season = t.one("SELECT * FROM seasons WHERE id = $1 FOR UPDATE", {param(seasonId)});
        if (season.is_null() || season.value("status", "") == "closed") return;

        LeaderboardService::snapshot("season", "season", seasonId, "");
        t.exec("UPDATE seasons SET status = 'closed' WHERE id = $1", {param(seasonId)});

        auto top = t.query(
            "SELECT sp.student_id, sp.season_xp, row_number() OVER (ORDER BY sp.season_xp DESC) "
            "AS rank FROM season_progress sp WHERE sp.season_id = $1 "
            "ORDER BY sp.season_xp DESC LIMIT 10",
            {param(seasonId)});
        for (const auto& r : top) {
            const int rank = r.value("rank", 0);
            const int coins = rank == 1 ? 500 : (rank == 2 ? 300 : (rank == 3 ? 150 : 50));
            t.exec("UPDATE students SET coins = coins + $1, updated_at = now() WHERE user_id = $2",
                   {param(coins), param(r["student_id"].get<std::string>())});
            svc::notify(t, r["student_id"].get<std::string>(), "season_ended",
                        json{{"kk", "Маусым аяқталды"}, {"ru", "Сезон завершён"},
                             {"en", "Season finished"}},
                        json{{"kk", "Сіз " + std::to_string(rank) + "-орынды иелендіңіз"},
                             {"ru", "Вы заняли " + std::to_string(rank) + "-е место"},
                             {"en", "You finished #" + std::to_string(rank)}},
                        {{"season_id", seasonId}, {"rank", rank}, {"coins", coins}});
        }
        Log::get()->info("season {} closed, {} winners", seasonId, top.size());
    });
}

} // namespace lumina
