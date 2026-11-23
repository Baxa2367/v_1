#pragma once
#include "services/common.h"

namespace lumina {

class TeacherService {
public:
    static json analytics(const AuthCtx& ctx, const std::string& courseId);
    static json courses(const AuthCtx& ctx);

    static json createCourse(const AuthCtx& ctx, const json& body);
    static json updateCourse(const AuthCtx& ctx, const std::string& id, const json& body);
    static json deleteCourse(const AuthCtx& ctx, const std::string& id);

    static json createLevel(const AuthCtx& ctx, const json& body);
    static json updateLevel(const AuthCtx& ctx, const std::string& id, const json& body);
    static json deleteLevel(const AuthCtx& ctx, const std::string& id);

    static json createTask(const AuthCtx& ctx, const json& body);
    static json updateTask(const AuthCtx& ctx, const std::string& id, const json& body);
    static json deleteTask(const AuthCtx& ctx, const std::string& id);
    static json importTasks(const AuthCtx& ctx, const json& body);

    static json topics(const AuthCtx& ctx, const std::string& courseId);
    static json createTopic(const AuthCtx& ctx, const json& body);

private:
    static void assertTeacherOfCourse(Tx& t, const AuthCtx& ctx, const std::string& courseId);
};

class AdminService {
public:
    static json users(const AuthCtx& ctx, const std::string& search, int limit, int offset);
    static json updateUser(const AuthCtx& ctx, const std::string& id, const json& body);
    static json anticheat(const AuthCtx& ctx, const std::string& status, int limit);
    static json resolveFlag(const AuthCtx& ctx, const std::string& id, const json& body);
    static json seasons(const AuthCtx& ctx);
    static json createSeason(const AuthCtx& ctx, const json& body);
    static json updateSeason(const AuthCtx& ctx, const std::string& id, const json& body);
    static json closeSeason(const AuthCtx& ctx, const std::string& id);
    static json createAchievement(const AuthCtx& ctx, const json& body);
    static json metrics(const AuthCtx& ctx);
    static json audit(const AuthCtx& ctx, int limit);
    static json economy(const AuthCtx& ctx, const json& body);
};

} // namespace lumina
