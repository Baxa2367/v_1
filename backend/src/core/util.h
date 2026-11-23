#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace lumina {

std::string isoNow();
std::string isoFromUnix(int64_t seconds);
int64_t unixNow();
// ISO-8601 with milliseconds, e.g. 2026-10-29T14:50:00.123Z
std::string isoNowMs();

std::string uuidV4();
std::string randomHex(size_t bytes);

// Base64 URL-safe without padding.
std::string base64UrlEncode(const std::string& in);
std::string base64UrlDecode(const std::string& in);
std::string base64Encode(const std::string& in);
std::string base64Decode(const std::string& in);

std::string toLower(std::string s);
std::string trimCopy(std::string s);
bool startsWith(const std::string& s, const std::string& p);

// "3,5" -> 3.5; returns false when not numeric.
bool tryParseNumber(const std::string& s, double& out);

std::string join(const std::vector<std::string>& v, const std::string& sep);

} // namespace lumina
