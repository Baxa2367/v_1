#include "core/crypto.h"
#include "core/errors.h"
#include "core/util.h"
#include <sodium.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <cstring>

namespace lumina {

void Crypto::init() {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium initialization failed");
    }
}

std::string Crypto::hashPassword(const std::string& password) {
    char hashed[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hashed, password.c_str(), password.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        throw internal("Password hashing failed");
    }
    return std::string(hashed);
}

bool Crypto::verifyPassword(const std::string& hash, const std::string& password) {
    if (hash.empty()) return false;
    return crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) == 0;
}

std::string Crypto::hmacSha256(const std::string& data, const std::string& key) {
    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(), key.data(), (int)key.size(),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(), out, &len);
    return std::string(reinterpret_cast<char*>(out), len);
}

bool Crypto::constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    return sodium_memcmp(a.data(), b.data(), a.size()) == 0;
}

std::string Crypto::sha256Hex(const std::string& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest);
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(SHA256_DIGEST_LENGTH * 2);
    for (unsigned char c : digest) {
        out += hex[c >> 4];
        out += hex[c & 15];
    }
    return out;
}

std::string Crypto::signJwt(const json& payload, const std::string& secret) {
    json header = {{"alg", "HS256"}, {"typ", "JWT"}};
    const std::string h = base64UrlEncode(header.dump());
    const std::string p = base64UrlEncode(payload.dump());
    const std::string signingInput = h + "." + p;
    const std::string sig = base64UrlEncode(hmacSha256(signingInput, secret));
    return signingInput + "." + sig;
}

json Crypto::verifyJwt(const std::string& token, const std::string& secret) {
    const auto first = token.find('.');
    const auto second = token.find('.', first == std::string::npos ? 0 : first + 1);
    if (first == std::string::npos || second == std::string::npos) {
        throw unauthorized("Malformed token");
    }
    const std::string signingInput = token.substr(0, second);
    const std::string sig = token.substr(second + 1);
    const std::string expected = base64UrlEncode(hmacSha256(signingInput, secret));
    if (!constantTimeEquals(sig, expected)) {
        throw unauthorized("Invalid token signature");
    }
    json payload;
    try {
        payload = json::parse(base64UrlDecode(token.substr(first + 1, second - first - 1)));
    } catch (...) {
        throw unauthorized("Malformed token payload");
    }
    if (!payload.contains("exp") || !payload["exp"].is_number()) {
        throw unauthorized("Token has no expiration");
    }
    if (payload["exp"].get<int64_t>() < unixNow()) {
        throw unauthorized("Token expired");
    }
    return payload;
}

} // namespace lumina
