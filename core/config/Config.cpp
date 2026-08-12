#include "core/config/Config.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <toml++/toml.hpp>

#include "core/logging/Logger.h"

namespace htb {

namespace {
std::filesystem::path defaultConfigPath() {
    const wchar_t* base = _wgetenv(L"LOCALAPPDATA");
    std::filesystem::path dir = (base && *base)
                                    ? std::filesystem::path(base) / L"HardwareToolbox"
                                    : std::filesystem::temp_directory_path() / L"HardwareToolbox";
    return dir / "config.toml";
}

int clampInt(int value, int lo, int hi) {
    return value < lo ? lo : (value > hi ? hi : value);
}
} // namespace

Config Config::load() {
    return loadFrom(defaultConfigPath());
}

Config Config::loadFrom(const std::filesystem::path& path) {
    Config cfg;
    cfg.m_path = path;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (!std::filesystem::exists(path)) {
        cfg.save();
        HTB_INFO("Created default config at {}", cfg.m_path.string());
        return cfg;
    }
    try {
        std::ifstream in(path, std::ios::binary);
        toml::table tbl = toml::parse(in);
        if (auto ui = tbl["ui"].as_table()) {
            if (auto v = ui->get_as<std::string>("theme")) cfg.ui.theme = v->get();
            if (auto v = ui->get_as<int64_t>("fps")) cfg.ui.fps = clampInt(static_cast<int>(v->get()), 30, 240);
        }
        if (auto mon = tbl["monitoring"].as_table()) {
            if (auto v = mon->get_as<int64_t>("interval_ms"))
                cfg.monitoring.interval_ms = clampInt(static_cast<int>(v->get()), 100, 60000);
        }
    } catch (const toml::parse_error& e) {
        HTB_ERROR("Failed to parse config {}: {}", path.string(), e.description());
    }
    return cfg;
}

void Config::save() const {
    toml::table tbl{
        {"ui", toml::table{{"theme", ui.theme}, {"fps", static_cast<int64_t>(ui.fps)}}},
        {"monitoring", toml::table{{"interval_ms", static_cast<int64_t>(monitoring.interval_ms)}}},
    };
    std::error_code ec;
    std::filesystem::create_directories(m_path.parent_path(), ec);
    std::ofstream out(m_path);
    if (out) {
        out << tbl << "\n";
    } else {
        HTB_ERROR("Failed to write config to {}", m_path.string());
    }
}

} // namespace htb
