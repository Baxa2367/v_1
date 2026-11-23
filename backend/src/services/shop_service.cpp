#include "services/shop_service.h"
#include "core/i18n.h"
#include "core/util.h"

namespace lumina {

namespace {

json localizeItem(const json& row, const std::string& locale) {
    json out = row;
    out["title"] = I18n::pick(row["title"], locale);
    out["description"] = I18n::pick(row["description"], locale);
    return out;
}

} // namespace

json ShopService::list(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        auto items = t.query("SELECT * FROM shop_items WHERE active ORDER BY sort_order, price_coins");
        auto inv = t.query("SELECT item_id, qty, equipped FROM inventory WHERE student_id = $1",
                           {param(ctx.userId)});
        std::map<std::string, json> owned;
        for (const auto& i : inv) owned[i["item_id"].get<std::string>()] = i;

        json out = json::array();
        for (const auto& item : items) {
            json x = localizeItem(item, ctx.locale);
            const std::string id = item["id"].get<std::string>();
            x["owned"] = owned.count(id) ? owned[id].value("qty", 0) : 0;
            x["equipped"] = owned.count(id) ? owned[id].value("equipped", false) : false;
            out.push_back(x);
        }
        auto balance = t.one("SELECT coins, gems FROM students WHERE user_id = $1",
                             {param(ctx.userId)});
        return json{{"items", out},
                    {"coins", balance.is_null() ? 0 : balance.value("coins", 0)},
                    {"gems", balance.is_null() ? 0 : balance.value("gems", 0)}};
    });
}

json ShopService::buy(const AuthCtx& ctx, const std::string& itemId) {
    return Db::instance().tx([&](Tx& t) {
        auto item = t.one("SELECT * FROM shop_items WHERE id = $1 AND active", {param(itemId)});
        if (item.is_null()) throw notFound("Shop item not found");
        auto student = t.one("SELECT coins, gems FROM students WHERE user_id = $1 FOR UPDATE",
                             {param(ctx.userId)});
        if (student.is_null()) throw notFound("Student not found");

        const int priceCoins = item.value("price_coins", 0);
        const int priceGems = item.value("price_gems", 0);
        if (student.value("coins", 0) < priceCoins || student.value("gems", 0) < priceGems) {
            throw badRequest("Not enough coins", {{"required_coins", priceCoins},
                                                  {"balance_coins", student.value("coins", 0)}});
        }
        t.exec("UPDATE students SET coins = coins - $1, gems = gems - $2, updated_at = now() "
               "WHERE user_id = $3",
               {param(priceCoins), param(priceGems), param(ctx.userId)});
        t.exec("INSERT INTO purchases (id, student_id, item_id, price_coins, price_gems) "
               "VALUES ($1, $2, $3, $4, $5)",
               {param(uuidV4()), param(ctx.userId), param(itemId), param(priceCoins), param(priceGems)});
        t.exec("INSERT INTO inventory (student_id, item_id, qty) VALUES ($1, $2, 1) "
               "ON CONFLICT (student_id, item_id) DO UPDATE SET qty = inventory.qty + 1",
               {param(ctx.userId), param(itemId)});
        auto left = t.one("SELECT coins FROM students WHERE user_id = $1", {param(ctx.userId)});
        return json{{"purchased", true},
                    {"item", localizeItem(item, ctx.locale)},
                    {"coins", left.value("coins", 0)}};
    });
}

json ShopService::inventory(const AuthCtx& ctx) {
    return Db::instance().tx([&](Tx& t) {
        auto rows = t.query(
            "SELECT i.qty, i.equipped, i.acquired_at, s.* FROM inventory i "
            "JOIN shop_items s ON s.id = i.item_id WHERE i.student_id = $1 ORDER BY s.kind, s.sort_order",
            {param(ctx.userId)});
        json out = json::array();
        for (const auto& r : rows) out.push_back(localizeItem(r, ctx.locale));
        return out;
    });
}

json ShopService::equip(const AuthCtx& ctx, const std::string& itemId, bool equipped) {
    return Db::instance().tx([&](Tx& t) {
        auto owned = t.scalar("SELECT 1 FROM inventory WHERE student_id = $1 AND item_id = $2",
                              {param(ctx.userId), param(itemId)});
        if (!owned) throw badRequest("Item is not owned");
        if (equipped) {
            t.exec("UPDATE inventory SET equipped = false WHERE student_id = $1 AND item_id IN "
                   "(SELECT id FROM shop_items WHERE kind = (SELECT kind FROM shop_items WHERE id = $2))",
                   {param(ctx.userId), param(itemId)});
        }
        t.exec("UPDATE inventory SET equipped = $3 WHERE student_id = $1 AND item_id = $2",
               {param(ctx.userId), param(itemId), param(equipped)});
        return json{{"equipped", equipped}};
    });
}

} // namespace lumina
