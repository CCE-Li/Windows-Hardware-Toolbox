#pragma once

#include <filesystem>
#include <string>

namespace htb {

struct UiConfig {
    std::string theme = "dark";
    int fps = 60;
};

struct MonitoringConfig {
    int interval_ms = 1000;
};

class Config {
public:
    static Config load();
    static Config loadFrom(const std::filesystem::path& path);

    void save() const;

    const std::filesystem::path& path() const { return m_path; }
    UiConfig ui;
    MonitoringConfig monitoring;

private:
    std::filesystem::path m_path;
};

} // namespace htb
