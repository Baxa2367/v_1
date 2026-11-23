#pragma once
#include "core/errors.h"
#include "db/db.h"
#include <string>

namespace lumina {

struct AuthCtx {
    std::string userId;
    std::string role;
    std::string locale;
    std::string displayName;
    bool isStudent() const { return role == "student"; }
    bool isTeacher() const { return role == "teacher" || role == "admin"; }
    bool isAdmin() const { return role == "admin"; }
};

namespace svc {

// Locale-aware representation of a student profile.
json loadStudent(Tx& t, const std::string& userId, const std::string& locale);
json loadUser(Tx& t, const std::string& userId, const std::string& locale);

// Public projection used in leaderboards / team lists.
json studentPublic(const json& studentRow, const std::string& locale);

std::string activeSeasonId(Tx& t);

void notify(Tx& t, const std::string& userId, const std::string& kind, const json& title,
            const json& body, const json& payload = json::object());

// Writes an append-only XP event and returns the new balance.
long long addXp(Tx& t, const std::string& studentId, int amount, const std::string& reason,
                const std::string& sourceType, const std::string& sourceId,
                const std::string& courseId, const std::string& seasonId, long long balanceBefore);

json xpProgress(long long totalXp, const std::string& locale);

void ensureEnrollment(Tx& t, const std::string& studentId, const std::string& courseId);

std::string todayLocal(Tx& t);
std::string weekStartLocal(Tx& t);

bool isUuid(const std::string& s);

} // namespace svc
} // namespace lumina
