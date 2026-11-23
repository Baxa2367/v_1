#pragma once
#include "services/common.h"

namespace lumina {

struct ReportFile {
    std::string contentType;
    std::string filename;
    std::string body;
    bool binary = false;
};

class ReportService {
public:
    static json progress(const AuthCtx& ctx);
    static json xp(const AuthCtx& ctx, int days);
    static json topics(const AuthCtx& ctx);
    static json achievements(const AuthCtx& ctx);
    static json leaderboard(const AuthCtx& ctx);
    static json heatmap(const AuthCtx& ctx, int days);
    static json overview(const AuthCtx& ctx);

    static ReportFile exportReport(const AuthCtx& ctx, const std::string& type,
                                   const std::string& format);
};

} // namespace lumina
