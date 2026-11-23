#include "services/notification_service.h"
#include "core/i18n.h"

namespace lumina {

json NotificationService::list(const AuthCtx& ctx, int limit) {
    limit = std::clamp(limit, 1, 100);
    return Db::instance().tx([&](Tx& t) {
        auto rows = t.query(
            "SELECT id, kind, title, body, payload, read_at, created_at FROM notifications "
            "WHERE user_id = $1 ORDER BY created_at DESC LIMIT $2",
            {param(ctx.userId), param(limit)});
        json out = json::array();
        for (const auto& r : rows) {
            json item = r;
            item["title"] = I18n::pick(r["title"], ctx.locale);
            item["body"] = I18n::pick(r["body"], ctx.locale);
            out.push_back(item);
        }
        return json{{"notifications", out},
                    {"unread", t.scalarInt("SELECT count(*) FROM notifications "
                                           "WHERE user_id = $1 AND read_at IS NULL",
                                           {param(ctx.userId)})}};
    });
}

void NotificationService::markRead(const AuthCtx& ctx, const std::string& id) {
    Db::instance().tx([&](Tx& t) {
        t.exec("UPDATE notifications SET read_at = now() WHERE id = $1 AND user_id = $2",
               {param(id), param(ctx.userId)});
    });
}

void NotificationService::markAllRead(const AuthCtx& ctx) {
    Db::instance().tx([&](Tx& t) {
        t.exec("UPDATE notifications SET read_at = now() WHERE user_id = $1 AND read_at IS NULL",
               {param(ctx.userId)});
    });
}

int NotificationService::unread(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        return static_cast<int>(t.scalarInt(
            "SELECT count(*) FROM notifications WHERE user_id = $1 AND read_at IS NULL",
            {param(ctx.userId)}));
    });
}

} // namespace lumina
