#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace lumina {

using json = nlohmann::json;

// Localisation helpers. Localised values in DB are JSON objects:
//   {"kk": "...", "ru": "...", "en": "..."}
// Resolution order: requested locale -> ru -> en -> kk -> first string.
class I18n {
public:
    static std::string normalize(const std::string& header); // "ru-RU,ru;q=0.9" -> "ru"
    static std::string pick(const json& value, const std::string& locale);
    static std::string pickOr(const json& value, const std::string& locale,
                              const std::string& fallback);
    // Replaces localised fields in-place with a plain string for API responses.
    static json localizeObject(const json& obj, const std::string& locale);
    static json localizeArray(const json& arr, const std::string& locale);
};

} // namespace lumina
