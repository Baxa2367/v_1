#pragma once
#include <memory>
#include <string>
#include <spdlog/spdlog.h>

namespace lumina {

class Log {
public:
    static void init(const std::string& level = "info", const std::string& file = "logs/lumina.log");
    static std::shared_ptr<spdlog::logger>& get();
};

} // namespace lumina
