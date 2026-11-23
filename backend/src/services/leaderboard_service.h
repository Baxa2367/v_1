#pragma once
#include "services/common.h"

namespace lumina {

class LeaderboardService {
public:
    static json query(const AuthCtx& ctx, const std::string& scope, const std::string& period,
                      const std::string& courseId, const std::string& seasonId, int limit,
                      int offset);
    static json myPosition(const AuthCtx& ctx, const std::string& scope,
                           const std::string& courseId, const std::string& seasonId);

    // Writes a daily snapshot (used for ▲▼ deltas and season closing).
    static void snapshot(const std::string& scope, const std::string& period,
                         const std::string& seasonId, const std::string& courseId);

    static json teamStandings(const AuthCtx& ctx, const std::string& period, int limit);
};

} // namespace lumina
