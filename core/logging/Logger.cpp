#include "core/logging/Logger.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <windows.h>

namespace htb {

namespace {
std::shared_ptr<spdlog::logger> g_logger;

std::filesystem::path defaultLogDir() {
    wchar_t* base = nullptr;
    size_t len = 0;
    _wdupenv_s(&base, &len, L"LOCALAPPDATA");
    std::filesystem::path dir = (base && *base)
                                    ? std::filesystem::path(base) / L"HardwareToolbox" / L"logs"
                                    : std::filesystem::temp_directory_path() / L"HardwareToolbox" / L"logs";
    free(base);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}
} // namespace

void Logger::init(bool attachConsole) {
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
    sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        (defaultLogDir() / "toolbox.log").string(), 1024 * 1024, 5));
    if (attachConsole) {
        if (AllocConsole()) {
            FILE* dummy = nullptr;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            sinks.push_back(std::make_shared<spdlog::sinks::stdout_sink_mt>());
        }
    }
    auto logger = std::make_shared<spdlog::logger>("htb", sinks.begin(), sinks.end());
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] %v");
#ifdef NDEBUG
    logger->set_level(spdlog::level::info);
#else
    logger->set_level(spdlog::level::trace);
#endif
    logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(logger);
    g_logger = std::move(logger);
}

std::shared_ptr<spdlog::logger> Logger::get() {
    if (g_logger) return g_logger;
    auto fallback = spdlog::default_logger();
    if (!fallback) {
        fallback = std::make_shared<spdlog::logger>("null", std::make_shared<spdlog::sinks::null_sink_mt>());
        spdlog::set_default_logger(fallback);
    }
    return fallback;
}

} // namespace htb
