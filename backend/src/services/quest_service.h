#pragma once
#include "services/common.h"

namespace lumina {

class QuestService {
public:
    // Ensures the student has today's daily + weekly quests and refreshes progress.
    static json list(const AuthCtx& ctx);
    static json claim(const AuthCtx& ctx, const std::string& studentQuestId);
    static void refresh(Tx& t, const std::string& studentId);
    static json onLevelCompleted(Tx& t, const std::string& studentId, const std::string& levelId);
};

} // namespace lumina
