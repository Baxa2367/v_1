#pragma once
#include "services/common.h"

namespace lumina {

class AiService {
public:
    static json nextTask(const AuthCtx& ctx, const std::string& courseId);
    static json explain(const AuthCtx& ctx, const std::string& decisionId);
    static json tips(const AuthCtx& ctx);
    static json forecast(const AuthCtx& ctx);
    static json generateTask(const AuthCtx& ctx, const json& body);
    static json mastery(const AuthCtx& ctx, const std::string& courseId);
    static json tipsForTeacher(Tx& t, const std::string& studentId);
};

} // namespace lumina
