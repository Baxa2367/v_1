#include "core/log.h"
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <filesystem>
#include <vector>

namespace lumina {

namespace {
std::shared_ptr<spdlog::logger> g_logger;
}

void Log::init(const std::string& level, const std::string& file) {
    std::vector<spdlog::sink_ptr> sinks;
    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console->set_pattern("%^%H:%M:%S.%e [%l]%$ %v");
    sinks.push_back(console);

    try {
        std::filesystem::create_directories(std::filesystem::path(file).parent_path());
        auto rotating = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file, 8 * 1024 * 1024, 3);
        rotating->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] [%t] %v");
        sinks.push_back(rotating);
    } catch (const std::exception&) {
        // file logging is best-effort
    }

    g_logger = std::make_shared<spdlog::logger>("lumina", sinks.begin(), sinks.end());
    g_logger->set_level(spdlog::level::from_str(level));
    g_logger->flush_on(spdlog::level::warn);
    spdlog::set_default_logger(g_logger);
}

std::shared_ptr<spdlog::logger>& Log::get() {
    if (!g_logger) init();
    return g_logger;
}

} // namespace lumina
