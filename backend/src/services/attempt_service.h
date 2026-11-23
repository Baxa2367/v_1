#pragma once
#include "services/common.h"

namespace lumina {

class AttemptService {
public:
    static json submit(const AuthCtx& ctx, const std::string& taskId, const json& body,
                       const std::string& ip);

    static json hint(const AuthCtx& ctx, const std::string& taskId, const json& body);
    static json history(const AuthCtx& ctx, int limit, int offset);
    static json taskForStudent(const AuthCtx& ctx, const std::string& taskId);
};

} // namespace lumina
