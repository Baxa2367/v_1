#pragma once
#include "services/common.h"

namespace lumina {

class ShopService {
public:
    static json list(const AuthCtx& ctx);
    static json buy(const AuthCtx& ctx, const std::string& itemId);
    static json inventory(const AuthCtx& ctx);
    static json equip(const AuthCtx& ctx, const std::string& itemId, bool equipped);
};

} // namespace lumina
