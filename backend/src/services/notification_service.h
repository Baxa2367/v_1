#pragma once
#include "services/common.h"

namespace lumina {

class NotificationService {
public:
    static json list(const AuthCtx& ctx, int limit);
    static void markRead(const AuthCtx& ctx, const std::string& id);
    static void markAllRead(const AuthCtx& ctx);
    static int unread(const AuthCtx& ctx);
};

} // namespace lumina
