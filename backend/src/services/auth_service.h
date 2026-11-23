#pragma once
#include "services/common.h"

namespace lumina {

class AuthService {
public:
    static json registerUser(const json& body, const std::string& ip, const std::string& ua);
    static json login(const json& body, const std::string& ip, const std::string& ua);
    static json refresh(const json& body, const std::string& ip, const std::string& ua);
    static void logout(const json& body);
    static json me(const AuthCtx& ctx);
    static json updateProfile(const AuthCtx& ctx, const json& body);
    static json changePassword(const AuthCtx& ctx, const json& body);

private:
    static json tokenResponse(Tx& t, const json& user, const std::string& ip, const std::string& ua);
};

} // namespace lumina
