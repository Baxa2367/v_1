#pragma once
#include "services/common.h"

namespace lumina {

class CourseService {
public:
    static json listCourses(const AuthCtx& ctx, const std::string& category,
                            const std::string& search, bool enrolledOnly);
    static json getCourse(const AuthCtx& ctx, const std::string& courseId);
    static json courseMap(const AuthCtx& ctx, const std::string& courseId);
    static json levelDetail(const AuthCtx& ctx, const std::string& levelId);
    static json enroll(const AuthCtx& ctx, const std::string& courseId);
    static json unenroll(const AuthCtx& ctx, const std::string& courseId);
    static json nextTask(const AuthCtx& ctx, const std::string& levelId,
                         const std::string& excludeTaskId);
    static json continueLearning(const AuthCtx& ctx);

    // Recomputes level completion/stars after an attempt. Returns the updated progress.
    static json recomputeLevelProgress(Tx& t, const std::string& studentId,
                                       const std::string& levelId, const std::string& locale);

    static bool levelUnlocked(Tx& t, const std::string& studentId, const json& level,
                              const json& previousLevel);
};

} // namespace lumina
