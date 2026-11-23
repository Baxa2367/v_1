#pragma once
#include "services/common.h"

namespace lumina {

class SeasonService {
public:
    static json current(const AuthCtx& ctx);
    static json list(const AuthCtx& ctx);
    static json rewards(const AuthCtx& ctx, const std::string& seasonId);
    static json claim(const AuthCtx& ctx, const std::string& seasonId, int tier);

    static void refreshTier(Tx& t, const std::string& seasonId, const std::string& studentId);
    static void closeSeason(const std::string& seasonId);
};

} // namespace lumina
