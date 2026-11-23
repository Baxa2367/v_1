#include "services/team_service.h"
#include "core/i18n.h"
#include "core/util.h"
#include "services/achievement_service.h"

namespace lumina {

namespace {

std::string inviteCode() {
    static const char* alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::string code;
    for (int i = 0; i < 6; ++i) code += alphabet[std::rand() % 32];
    return code;
}

json teamRow(Tx& t, const std::string& teamId, const std::string& locale) {
    auto team = t.one("SELECT * FROM teams WHERE id = $1", {param(teamId)});
    if (team.is_null()) return nullptr;
    auto members = t.query(
        "SELECT tm.student_id, tm.role, tm.joined_at, s.nickname, s.avatar, s.level, s.total_xp "
        "FROM team_members tm JOIN students s ON s.user_id = tm.student_id "
        "WHERE tm.team_id = $1 ORDER BY tm.role DESC, s.total_xp DESC",
        {param(teamId)});
    json arr = json::array();
    for (const auto& m : members) arr.push_back(m);
    team["members"] = arr;
    team["member_count"] = arr.size();
    team["total_xp"] = t.scalarInt(
        "SELECT coalesce(sum(s.total_xp), 0) FROM team_members tm "
        "JOIN students s ON s.user_id = tm.student_id WHERE tm.team_id = $1",
        {param(teamId)});
    return team;
}

} // namespace

json TeamService::mine(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        auto row = t.one("SELECT team_id FROM team_members WHERE student_id = $1",
                         {param(ctx.userId)});
        if (row.is_null()) return json{{"team", nullptr}};
        return json{{"team", teamRow(t, row["team_id"].get<std::string>(), ctx.locale)}};
    });
}

json TeamService::list(const AuthCtx& ctx) {
    (void)ctx;
    return Db::instance().tx([&](Tx& t) {
        auto rows = t.query(
            "SELECT t.*, (SELECT count(*) FROM team_members tm WHERE tm.team_id = t.id) AS members, "
            "(SELECT coalesce(sum(s.total_xp), 0) FROM team_members tm "
            " JOIN students s ON s.user_id = tm.student_id WHERE tm.team_id = t.id) AS total_xp "
            "FROM teams t ORDER BY total_xp DESC LIMIT 50");
        return json{{"teams", rows}};
    });
}

json TeamService::create(const AuthCtx& ctx, const json& body) {
    const std::string name = trimCopy(body.value("name", ""));
    if (name.size() < 2 || name.size() > 40) throw unprocessable("Team name must be 2..40 chars");
    const std::string emblem = body.value("emblem", "groups");
    const std::string description = body.value("description", "");

    return Db::instance().tx([&](Tx& t) {
        auto existing = t.scalar("SELECT 1 FROM team_members WHERE student_id = $1",
                                 {param(ctx.userId)});
        if (existing) throw conflict("You are already in a team");

        const std::string teamId = uuidV4();
        std::string code = inviteCode();
        for (int i = 0; i < 5; ++i) {
            if (!t.scalar("SELECT 1 FROM teams WHERE invite_code = $1", {param(code)})) break;
            code = inviteCode();
        }
        t.exec("INSERT INTO teams (id, name, emblem, description, captain_id, invite_code) "
               "VALUES ($1, $2, $3, $4, $5, $6)",
               {param(teamId), param(name), param(emblem), param(description), param(ctx.userId),
                param(code)});
        t.exec("INSERT INTO team_members (team_id, student_id, role) VALUES ($1, $2, 'captain')",
               {param(teamId), param(ctx.userId)});
        AchievementService::evaluate(t, ctx.userId, ctx.locale, "team_joined");
        return json{{"team", teamRow(t, teamId, ctx.locale)}};
    });
}

json TeamService::join(const AuthCtx& ctx, const json& body) {
    const std::string code = toLower(trimCopy(body.value("invite_code", "")));
    return Db::instance().tx([&](Tx& t) {
        auto team = t.one("SELECT * FROM teams WHERE lower(invite_code) = $1", {param(code)});
        if (team.is_null()) throw notFound("Team with this code not found");
        auto existing = t.scalar("SELECT 1 FROM team_members WHERE student_id = $1",
                                 {param(ctx.userId)});
        if (existing) throw conflict("You are already in a team");
        t.exec("INSERT INTO team_members (team_id, student_id) VALUES ($1, $2)",
               {param(team["id"].get<std::string>()), param(ctx.userId)});
        AchievementService::evaluate(t, ctx.userId, ctx.locale, "team_joined");
        return json{{"team", teamRow(t, team["id"].get<std::string>(), ctx.locale)}};
    });
}

json TeamService::leave(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        auto member = t.one("SELECT team_id, role FROM team_members WHERE student_id = $1 FOR UPDATE",
                            {param(ctx.userId)});
        if (member.is_null()) throw badRequest("You are not in a team");
        const std::string teamId = member["team_id"].get<std::string>();
        t.exec("DELETE FROM team_members WHERE student_id = $1", {param(ctx.userId)});
        if (member.value("role", "") == "captain") {
            auto next = t.one("SELECT student_id FROM team_members WHERE team_id = $1 "
                              "ORDER BY joined_at LIMIT 1",
                              {param(teamId)});
            if (next.is_null()) {
                t.exec("DELETE FROM teams WHERE id = $1", {param(teamId)});
            } else {
                t.exec("UPDATE team_members SET role = 'captain' WHERE team_id = $1 AND student_id = $2",
                       {param(teamId), param(next["student_id"].get<std::string>())});
                t.exec("UPDATE teams SET captain_id = $1 WHERE id = $2",
                       {param(next["student_id"].get<std::string>()), param(teamId)});
            }
        }
        return json{{"left", true}};
    });
}

json TeamService::detail(const AuthCtx& ctx, const std::string& teamId) {
    return Db::instance().tx([&](Tx& t) {
        auto team = teamRow(t, teamId, ctx.locale);
        if (team.is_null()) throw notFound("Team not found");
        return json{{"team", team}};
    });
}

json TeamService::tournaments(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        auto rows = t.query(
            "SELECT tr.*, "
            "(SELECT count(*) FROM tournament_teams tt WHERE tt.tournament_id = tr.id) AS teams "
            "FROM tournaments tr ORDER BY tr.starts_at DESC");
        json out = json::array();
        auto myTeam = t.scalar("SELECT team_id FROM team_members WHERE student_id = $1",
                               {param(ctx.userId)});
        for (const auto& r : rows) {
            json item = r;
            item["title"] = I18n::pick(r["title"], ctx.locale);
            item["description"] = I18n::pick(r["description"], ctx.locale);
            item["my_rank"] = nullptr;
            if (myTeam) {
                auto rank = t.one("SELECT rank, score FROM tournament_teams "
                                  "WHERE tournament_id = $1 AND team_id = $2",
                                  {param(r["id"].get<std::string>()), param(*myTeam)});
                if (!rank.is_null()) item["my_rank"] = rank;
            }
            out.push_back(item);
        }
        return json{{"tournaments", out}};
    });
}

json TeamService::standings(const AuthCtx& ctx, const std::string& tournamentId) {
    return Db::instance().tx([&](Tx& t) {
        auto tr = t.one("SELECT * FROM tournaments WHERE id = $1", {param(tournamentId)});
        if (tr.is_null()) throw notFound("Tournament not found");
        auto rows = t.query(
            "SELECT tt.team_id, tm.name, tm.emblem, tt.score, tt.rank, "
            "(SELECT count(*) FROM team_members x WHERE x.team_id = tm.id) AS members "
            "FROM tournament_teams tt JOIN teams tm ON tm.id = tt.team_id "
            "WHERE tt.tournament_id = $1 ORDER BY tt.score DESC",
            {param(tournamentId)});
        json entries = json::array();
        int rank = 1;
        for (const auto& r : rows) {
            entries.push_back({{"rank", r.value("rank", rank)},
                               {"team_id", r["team_id"]},
                               {"name", r["name"]},
                               {"emblem", r["emblem"]},
                               {"score", r.value("score", 0.0)},
                               {"members", r.value("members", 0)}});
            ++rank;
        }
        json tournamentJson = tr;
        tournamentJson["title"] = I18n::pick(tr["title"], ctx.locale);
        tournamentJson["description"] = I18n::pick(tr["description"], ctx.locale);
        return json{{"tournament", tournamentJson}, {"standings", entries}};
    });
}

} // namespace lumina

