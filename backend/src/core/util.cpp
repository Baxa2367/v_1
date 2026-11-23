#include "core/util.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>
#include <cmath>
#include <cctype>

namespace lumina {

namespace {
const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const char* kB64Url = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string b64(const std::string& in, const char* alphabet, bool pad) {
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= in.size()) {
        uint32_t n = (uint8_t)in[i] << 16 | (uint8_t)in[i + 1] << 8 | (uint8_t)in[i + 2];
        out += alphabet[(n >> 18) & 63];
        out += alphabet[(n >> 12) & 63];
        out += alphabet[(n >> 6) & 63];
        out += alphabet[n & 63];
        i += 3;
    }
    const size_t rem = in.size() - i;
    if (rem == 1) {
        uint32_t n = (uint8_t)in[i] << 16;
        out += alphabet[(n >> 18) & 63];
        out += alphabet[(n >> 12) & 63];
        if (pad) out += "==";
    } else if (rem == 2) {
        uint32_t n = (uint8_t)in[i] << 16 | (uint8_t)in[i + 1] << 8;
        out += alphabet[(n >> 18) & 63];
        out += alphabet[(n >> 12) & 63];
        out += alphabet[(n >> 6) & 63];
        if (pad) out += '=';
    }
    return out;
}

std::string unb64(const std::string& in) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+' || c == '-') return 62;
        if (c == '/' || c == '_') return 63;
        return -1;
    };
    std::string out;
    std::vector<int> buf;
    for (char c : in) {
        if (c == '=' || c == '\n' || c == '\r') continue;
        int v = val(c);
        if (v < 0) continue;
        buf.push_back(v);
        if (buf.size() == 4) {
            uint32_t n = buf[0] << 18 | buf[1] << 12 | buf[2] << 6 | buf[3];
            out += char((n >> 16) & 0xFF);
            out += char((n >> 8) & 0xFF);
            out += char(n & 0xFF);
            buf.clear();
        }
    }
    if (buf.size() == 2) {
        uint32_t n = buf[0] << 18 | buf[1] << 12;
        out += char((n >> 16) & 0xFF);
    } else if (buf.size() == 3) {
        uint32_t n = buf[0] << 18 | buf[1] << 12 | buf[2] << 6;
        out += char((n >> 16) & 0xFF);
        out += char((n >> 8) & 0xFF);
    }
    return out;
}
} // namespace

std::string isoFromUnix(int64_t seconds) {
    std::time_t t = static_cast<std::time_t>(seconds);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string isoNow() { return isoFromUnix(unixNow()); }

int64_t unixNow() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string isoNowMs() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[40];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    std::snprintf(buf + std::strlen(buf), sizeof(buf) - std::strlen(buf), ".%03lldZ",
                  (long long)ms.count());
    return buf;
}

std::string uuidV4() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    uint64_t a = rng(), b = rng();
    a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%08llx-%04llx-%04llx-%04llx-%012llx",
                  (unsigned long long)(a >> 32), (unsigned long long)((a >> 16) & 0xFFFF),
                  (unsigned long long)(a & 0xFFFF), (unsigned long long)(b >> 48),
                  (unsigned long long)(b & 0xFFFFFFFFFFFFULL));
    return buf;
}

std::string randomHex(size_t bytes) {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(bytes * 2);
    for (size_t i = 0; i < bytes; ++i) {
        unsigned v = (unsigned)(rng() & 0xFF);
        out += hex[v >> 4];
        out += hex[v & 15];
    }
    return out;
}

std::string base64UrlEncode(const std::string& in) { return b64(in, kB64Url, false); }
std::string base64UrlDecode(const std::string& in) { return unb64(in); }
std::string base64Encode(const std::string& in) { return b64(in, kB64, true); }
std::string base64Decode(const std::string& in) { return unb64(in); }

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string trimCopy(std::string s) {
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

bool startsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

bool tryParseNumber(const std::string& s, double& out) {
    std::string t = trimCopy(s);
    if (t.empty()) return false;
    std::replace(t.begin(), t.end(), ',', '.');
    char* end = nullptr;
    errno = 0;
    double v = std::strtod(t.c_str(), &end);
    if (errno != 0 || end == t.c_str()) return false;
    while (*end && std::isspace((unsigned char)*end)) ++end;
    if (*end != '\0') return false;
    out = v;
    return true;
}

std::string join(const std::vector<std::string>& v, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += sep;
        out += v[i];
    }
    return out;
}

} // namespace lumina

