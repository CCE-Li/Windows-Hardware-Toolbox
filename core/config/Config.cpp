#include "core/config/Config.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <toml++/toml.hpp>

#include "core/logging/Logger.h"

namespace htb {

namespace {
std::filesystem::path defaultConfigPath() {
    wchar_t* base = nullptr;
    size_t len = 0;
    _wdupenv_s(&base, &len, L"LOCALAPPDATA");
    std::filesystem::path dir = (base && *base)
                                    ? std::filesystem::path(base) / L"HardwareToolbox"
                                    : std::filesystem::temp_directory_path() / L"HardwareToolbox";
    free(base);
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
        if (auto cam = tbl["camera"].as_table()) {
            if (auto v = cam->get_as<int64_t>("camera_index")) cfg.camera.cameraIndex = static_cast<int>(v->get());
            if (auto v = cam->get_as<int64_t>("rotation")) cfg.camera.rotation = static_cast<int>(v->get());
            if (auto v = cam->get_as<double>("zoom")) cfg.camera.zoom = static_cast<float>(v->get());
            if (auto v = cam->get_as<double>("pan_x")) cfg.camera.panX = static_cast<float>(v->get());
            if (auto v = cam->get_as<double>("pan_y")) cfg.camera.panY = static_cast<float>(v->get());
            if (auto v = cam->get_as<bool>("flip_h")) cfg.camera.flipHorizontal = v->get();
            if (auto v = cam->get_as<bool>("flip_v")) cfg.camera.flipVertical = v->get();
            if (auto v = cam->get_as<int64_t>("brightness")) cfg.camera.brightness = static_cast<int>(v->get());
            if (auto v = cam->get_as<int64_t>("contrast")) cfg.camera.contrast = static_cast<int>(v->get());
            if (auto v = cam->get_as<int64_t>("saturation")) cfg.camera.saturation = static_cast<int>(v->get());
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
        {"camera",
         toml::table{{"camera_index", static_cast<int64_t>(camera.cameraIndex)},
                     {"rotation", static_cast<int64_t>(camera.rotation)},
                     {"zoom", static_cast<double>(camera.zoom)},
                     {"pan_x", static_cast<double>(camera.panX)},
                     {"pan_y", static_cast<double>(camera.panY)},
                     {"flip_h", camera.flipHorizontal},
                     {"flip_v", camera.flipVertical},
                     {"brightness", static_cast<int64_t>(camera.brightness)},
                     {"contrast", static_cast<int64_t>(camera.contrast)},
                     {"saturation", static_cast<int64_t>(camera.saturation)}}},
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
