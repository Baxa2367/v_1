#include "services/auth_service.h"
#include "core/config.h"
#include "core/crypto.h"
#include "core/i18n.h"
#include "core/util.h"
#include <regex>

namespace lumina {

namespace {

constexpr int kMinPassword = 8;

bool validEmail(const std::string& e) {
    static const std::regex re(R"(^[A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}$)");
    return std::regex_match(e, re);
}

std::string secret() { return Config::instance().get("JWT_SECRET"); }

json studentSession(Tx& t, const std::string& userId, const std::string& locale) {
    auto user = svc::loadUser(t, userId, locale);
    const std::string role = user.value("role", "student");
    if (role == "student") return svc::loadStudent(t, userId, locale);

    json profile = user;
    profile["nickname"] = profile.value("display_name", "");
    profile["avatar"] = "admin_panel_settings";
    profile["total_xp"] = 0;
    profile["level"] = 1;
    profile["coins"] = 0;
    return profile;
}

std::string issueAccess(const std::string& userId, const std::string& role,
                        const std::string& name, const std::string& locale) {
    const int ttlMin = Config::instance().getInt("ACCESS_TTL_MIN", 60);
    json payload = {
        {"sub", userId},
        {"role", role},
        {"name", name},
        {"locale", locale},
        {"typ", "access"},
        {"iat", unixNow()},
        {"exp", unixNow() + ttlMin * 60},
        {"jti", uuidV4()},
    };
    return Crypto::signJwt(payload, secret());
}

std::string issueRefresh(Tx& t, const std::string& userId, const std::string& ip,
                         const std::string& ua) {
    const int days = Config::instance().getInt("REFRESH_TTL_DAYS", 30);
    const std::string token = randomHex(32);
    t.exec("INSERT INTO refresh_tokens (id, user_id, token_hash, expires_at, user_agent, ip) "
           "VALUES ($1, $2, $3, now() + ($4 || ' days')::interval, $5, $6)",
           {param(uuidV4()), param(userId), param(Crypto::sha256Hex(token)), param(days),
            param(ua.substr(0, 400)), param(ip)});
    return token;
}

} // namespace

json AuthService::tokenResponse(Tx& t, const json& user, const std::string& ip,
                                const std::string& ua) {
    const std::string locale = user.value("locale", "kk");
    const std::string access = issueAccess(user["id"].get<std::string>(), user["role"],
                                           user.value("display_name", ""), locale);
    const std::string refresh = issueRefresh(t, user["id"].get<std::string>(), ip, ua);
    json profile = studentSession(t, user["id"].get<std::string>(), locale);
    return {
        {"access_token", access},
        {"refresh_token", refresh},
        {"token_type", "Bearer"},
        {"expires_in", Config::instance().getInt("ACCESS_TTL_MIN", 60) * 60},
        {"user", {{"id", user["id"]}, {"email", user["email"]}, {"role", user["role"]},
                  {"locale", locale}, {"display_name", user.value("display_name", "")}}},
        {"profile", profile},
    };
}

json AuthService::registerUser(const json& body, const std::string& ip, const std::string& ua) {
    const std::string email = toLower(trimCopy(body.value("email", "")));
    const std::string password = body.value("password", "");
    const std::string displayName = trimCopy(body.value("display_name", ""));
    const std::string locale = I18n::normalize(body.value("locale", "kk"));
    const std::string nickname = trimCopy(body.value("nickname", displayName));
    const std::string avatar = body.value("avatar", "face");

    json errors = json::object();
    if (!validEmail(email)) errors["email"] = "invalid_email";
    if (password.size() < kMinPassword) errors["password"] = "too_short";
    if (nickname.size() < 2) errors["nickname"] = "too_short";
    if (!errors.empty()) throw unprocessable("Please check the registration form", errors);

    const std::string userId = uuidV4();
    const std::string hash = Crypto::hashPassword(password);

    return Db::instance().tx([&](Tx& t) {
        auto exists = t.scalar("SELECT 1 FROM users WHERE lower(email) = $1 AND deleted_at IS NULL",
                               {param(email)});
        if (exists) throw conflict("Email already registered", {{"email", "taken"}});

        t.exec("INSERT INTO users (id, email, password_hash, role, locale, display_name) "
               "VALUES ($1, $2, $3, 'student', $4, $5)",
               {param(userId), param(email), param(hash), param(locale), param(displayName)});
        t.exec("INSERT INTO students (user_id, nickname, avatar) VALUES ($1, $2, $3)",
               {param(userId), param(nickname.substr(0, 40)), param(avatar)});

        // Assign the starter daily quests right away so the dashboard is alive.
        json user = {{"id", userId}, {"email", email}, {"role", "student"},
                     {"locale", locale}, {"display_name", displayName}};
        t.exec("INSERT INTO audit_log (actor_id, action, entity_type, entity_id, payload, ip) "
               "VALUES ($1, 'user_registered', 'user', $2, $3::jsonb, $4)",
               {param(userId), param(userId), paramJson({{"email", email}}), param(ip)});
        return tokenResponse(t, user, ip, ua);
    });
}

json AuthService::login(const json& body, const std::string& ip, const std::string& ua) {
    const std::string email = toLower(trimCopy(body.value("email", "")));
    const std::string password = body.value("password", "");
    if (email.empty() || password.empty()) throw unprocessable("Email and password are required");

    return Db::instance().tx([&](Tx& t) {
        auto user = t.one("SELECT id, email, password_hash, role, locale, display_name, status "
                          "FROM users WHERE lower(email) = $1 AND deleted_at IS NULL",
                          {param(email)});
        if (user.is_null() || !Crypto::verifyPassword(user["password_hash"].get<std::string>(), password)) {
            throw unauthorized("Invalid email or password");
        }
        if (user["status"] == "blocked") throw forbidden("Account is blocked");
        t.exec("UPDATE users SET updated_at = now() WHERE id = $1", {param(user["id"].get<std::string>())});
        t.exec("UPDATE students SET last_active_at = now() WHERE user_id = $1",
               {param(user["id"].get<std::string>())});
        return tokenResponse(t, user, ip, ua);
    });
}

json AuthService::refresh(const json& body, const std::string& ip, const std::string& ua) {
    const std::string token = body.value("refresh_token", "");
    if (token.empty()) throw unprocessable("refresh_token is required");

    return Db::instance().tx([&](Tx& t) {
        auto row = t.one("SELECT rt.id, rt.user_id, rt.expires_at, rt.revoked_at, u.email, u.role, "
                         "u.locale, u.display_name, u.status "
                         "FROM refresh_tokens rt JOIN users u ON u.id = rt.user_id "
                         "WHERE rt.token_hash = $1",
                         {param(Crypto::sha256Hex(token))});
        if (row.is_null()) throw unauthorized("Unknown refresh token");
        if (!row["revoked_at"].is_null()) throw unauthorized("Refresh token already used");
        if (row["status"] == "blocked") throw forbidden("Account is blocked");

        const std::string expires = row["expires_at"].get<std::string>();
        if (expires < isoNow()) throw unauthorized("Refresh token expired");

        t.exec("UPDATE refresh_tokens SET revoked_at = now() WHERE id = $1",
               {param(row["id"].get<std::string>())});
        json user = {{"id", row["user_id"]}, {"email", row["email"]}, {"role", row["role"]},
                     {"locale", row["locale"]}, {"display_name", row["display_name"]}};
        return tokenResponse(t, user, ip, ua);
    });
}

void AuthService::logout(const json& body) {
    const std::string token = body.value("refresh_token", "");
    if (token.empty()) return;
    Db::instance().tx([&](Tx& t) {
        t.exec("UPDATE refresh_tokens SET revoked_at = now() "
               "WHERE token_hash = $1 AND revoked_at IS NULL",
               {param(Crypto::sha256Hex(token))});
    });
}

json AuthService::me(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        auto user = svc::loadUser(t, ctx.userId, ctx.locale);
        json out = {{"user", user}};
        if (ctx.role == "student") {
            out["profile"] = svc::loadStudent(t, ctx.userId, ctx.locale);
        }
        auto unread = t.scalarInt(
            "SELECT count(*) FROM notifications WHERE user_id = $1 AND read_at IS NULL",
            {param(ctx.userId)});
        out["unread_notifications"] = unread;
        return out;
    });
}

json AuthService::updateProfile(const AuthCtx& ctx, const json& body) {
    return Db::instance().tx([&](Tx& t) {
        if (body.contains("locale")) {
            const std::string locale = I18n::normalize(body["locale"].get<std::string>());
            t.exec("UPDATE users SET locale = $1, updated_at = now() WHERE id = $2",
                   {param(locale), param(ctx.userId)});
        }
        if (body.contains("display_name")) {
            t.exec("UPDATE users SET display_name = $1, updated_at = now() WHERE id = $2",
                   {param(trimCopy(body["display_name"].get<std::string>())), param(ctx.userId)});
        }
        if (ctx.role == "student") {
            if (body.contains("nickname")) {
                t.exec("UPDATE students SET nickname = $1, updated_at = now() WHERE user_id = $2",
                       {param(body["nickname"].get<std::string>().substr(0, 40)), param(ctx.userId)});
            }
            if (body.contains("avatar")) {
                t.exec("UPDATE students SET avatar = $1, updated_at = now() WHERE user_id = $2",
                       {param(body["avatar"].get<std::string>()), param(ctx.userId)});
            }
            if (body.contains("bio")) {
                t.exec("UPDATE students SET bio = $1, updated_at = now() WHERE user_id = $2",
                       {param(body["bio"].get<std::string>().substr(0, 300)), param(ctx.userId)});
            }
            if (body.contains("daily_goal_xp")) {
                const int goal = body["daily_goal_xp"].get<int>();
                if (goal < 10 || goal > 500) throw unprocessable("daily_goal_xp must be 10..500");
                t.exec("UPDATE students SET daily_goal_xp = $1, updated_at = now() WHERE user_id = $2",
                       {param(goal), param(ctx.userId)});
            }
            if (body.contains("privacy_flags")) {
                t.exec("UPDATE students SET privacy_flags = $1::jsonb, updated_at = now() "
                       "WHERE user_id = $2",
                       {paramJson(body["privacy_flags"]), param(ctx.userId)});
            }
            if (body.contains("showcase")) {
                t.exec("UPDATE students SET showcase = $1::jsonb, updated_at = now() WHERE user_id = $2",
                       {paramJson(body["showcase"]), param(ctx.userId)});
            }
        }
        json out = {{"user", svc::loadUser(t, ctx.userId, ctx.locale)}};
        if (ctx.role == "student") out["profile"] = svc::loadStudent(t, ctx.userId, ctx.locale);
        return out;
    });
}

json AuthService::changePassword(const AuthCtx& ctx, const json& body) {
    const std::string current = body.value("current_password", "");
    const std::string next = body.value("new_password", "");
    if (next.size() < kMinPassword) throw unprocessable("Password is too short");
    return Db::instance().tx([&](Tx& t) {
        auto hash = t.scalar("SELECT password_hash FROM users WHERE id = $1", {param(ctx.userId)});
        if (!hash || !Crypto::verifyPassword(*hash, current)) {
            throw unauthorized("Current password is incorrect");
        }
        t.exec("UPDATE users SET password_hash = $1, updated_at = now() WHERE id = $2",
               {param(Crypto::hashPassword(next)), param(ctx.userId)});
        t.exec("UPDATE refresh_tokens SET revoked_at = now() "
               "WHERE user_id = $1 AND revoked_at IS NULL",
               {param(ctx.userId)});
        return json{{"ok", true}};
    });
}

} // namespace lumina
