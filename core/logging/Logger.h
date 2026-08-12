#pragma once

#include <memory>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

namespace htb {

class Logger {
public:
    static void init(bool attachConsole);
    static std::shared_ptr<spdlog::logger> get();
};

} // namespace htb

#define HTB_TRACE(...) SPDLOG_LOGGER_CALL(htb::Logger::get().get(), spdlog::level::trace, __VA_ARGS__)
#define HTB_DEBUG(...) SPDLOG_LOGGER_CALL(htb::Logger::get().get(), spdlog::level::debug, __VA_ARGS__)
#define HTB_INFO(...) SPDLOG_LOGGER_CALL(htb::Logger::get().get(), spdlog::level::info, __VA_ARGS__)
#define HTB_WARN(...) SPDLOG_LOGGER_CALL(htb::Logger::get().get(), spdlog::level::warn, __VA_ARGS__)
#define HTB_ERROR(...) SPDLOG_LOGGER_CALL(htb::Logger::get().get(), spdlog::level::err, __VA_ARGS__)
#define HTB_CRITICAL(...) SPDLOG_LOGGER_CALL(htb::Logger::get().get(), spdlog::level::critical, __VA_ARGS__)
