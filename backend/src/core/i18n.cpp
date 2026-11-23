#include "core/i18n.h"
#include "core/util.h"
#include <algorithm>

namespace lumina {

std::string I18n::normalize(const std::string& header) {
    if (header.empty()) return "kk";
    std::string first = header.substr(0, header.find(','));
    first = trimCopy(first.substr(0, first.find(';')));
    std::string lower = toLower(first);
    if (startsWith(lower, "kk") || startsWith(lower, "kz")) return "kk";
    if (startsWith(lower, "ru")) return "ru";
    if (startsWith(lower, "en")) return "en";
    return "kk";
}

std::string I18n::pick(const json& value, const std::string& locale) {
    if (value.is_string()) return value.get<std::string>();
    if (!value.is_object()) return "";
    static const char* order[] = {"kk", "ru", "en"};
    if (value.contains(locale) && value[locale].is_string() &&
        !value[locale].get<std::string>().empty()) {
        return value[locale].get<std::string>();
    }
    for (const char* key : order) {
        if (value.contains(key) && value[key].is_string() &&
            !value[key].get<std::string>().empty()) {
            return value[key].get<std::string>();
        }
    }
    for (auto it = value.begin(); it != value.end(); ++it) {
        if (it.value().is_string()) return it.value().get<std::string>();
    }
    return "";
}

std::string I18n::pickOr(const json& value, const std::string& locale,
                         const std::string& fallback) {
    auto s = pick(value, locale);
    return s.empty() ? fallback : s;
}

json I18n::localizeObject(const json& obj, const std::string& locale) {
    if (!obj.is_object()) return obj;
    json out = json::object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (it.value().is_object() && (it.value().contains("kk") || it.value().contains("ru") ||
                                       it.value().contains("en"))) {
            out[it.key()] = pick(it.value(), locale);
        } else if (it.value().is_array()) {
            out[it.key()] = localizeArray(it.value(), locale);
        } else if (it.value().is_object()) {
            out[it.key()] = localizeObject(it.value(), locale);
        } else {
            out[it.key()] = it.value();
        }
    }
    return out;
}

json I18n::localizeArray(const json& arr, const std::string& locale) {
    json out = json::array();
    for (const auto& item : arr) {
        if (item.is_object()) out.push_back(localizeObject(item, locale));
        else out.push_back(item);
    }
    return out;
}

} // namespace lumina
