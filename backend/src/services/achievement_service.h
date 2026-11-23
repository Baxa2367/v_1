#pragma once
#include "services/common.h"

namespace lumina {

class AchievementService {
public:
    // Evaluates rules for `event` and awards newly earned achievements.
    // Returns a JSON array of awarded achievement objects (localised).
    static json evaluate(Tx& t, const std::string& studentId, const std::string& locale,
                         const std::string& event, const json& extra = json::object());

    // All achievements with progress for the collection screen.
    static json collection(const AuthCtx& ctx);

    static json snapshot(Tx& t, const std::string& studentId);

private:
    static bool matches(const json& rule, const std::string& event);
    static double metricValue(const json& metrics, const std::string& name);
};

} // namespace lumina
