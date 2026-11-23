#pragma once
#include "services/common.h"

namespace lumina {

class TeamService {
public:
    static json mine(const AuthCtx& ctx);
    static json list(const AuthCtx& ctx);
    static json create(const AuthCtx& ctx, const json& body);
    static json join(const AuthCtx& ctx, const json& body);
    static json leave(const AuthCtx& ctx);
    static json detail(const AuthCtx& ctx, const std::string& teamId);
    static json tournaments(const AuthCtx& ctx);
    static json standings(const AuthCtx& ctx, const std::string& tournamentId);
};

} // namespace lumina
