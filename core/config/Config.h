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

struct CameraConfig {
    int cameraIndex = 0;
    int rotation = 0;
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    bool flipHorizontal = false;
    bool flipVertical = false;
    int brightness = 0;
    int contrast = 0;
    int saturation = 0;
};

class Config {
public:
    static Config load();
    static Config loadFrom(const std::filesystem::path& path);

    void save() const;

    const std::filesystem::path& path() const { return m_path; }
    UiConfig ui;
    MonitoringConfig monitoring;
    CameraConfig camera;

private:
    std::filesystem::path m_path;
};

} // namespace htb
