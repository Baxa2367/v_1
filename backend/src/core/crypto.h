#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace lumina {

using json = nlohmann::json;

class Crypto {
public:
    static void init();

    // Argon2id (libsodium crypto_pwhash_str)
    static std::string hashPassword(const std::string& password);
    static bool verifyPassword(const std::string& hash, const std::string& password);

    static std::string hmacSha256(const std::string& data, const std::string& key);
    static bool constantTimeEquals(const std::string& a, const std::string& b);
    static std::string sha256Hex(const std::string& data);

    // JWT (HS256)
    static std::string signJwt(const json& payload, const std::string& secret);
    static json verifyJwt(const std::string& token, const std::string& secret);
};

} // namespace lumina
