#pragma once

#include <atomic>
#include <memory>
#include <string>

namespace htb {

struct CameraEngineParams {
    int cameraIndex = 0;
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    bool flipHorizontal = false;
    bool flipVertical = false;
    int brightness = 0;
    int contrast = 0;
    int saturation = 0;
};

struct CameraEngineStatus {
    bool running = false;
    double fps = 0.0;
    uint64_t frames = 0;
    std::string error;
    std::string source;
};

class CameraEngineController {
public:
    CameraEngineController();
    ~CameraEngineController();

    void start(const CameraEngineParams& params);
    void updateParams(const CameraEngineParams& params);
    void stop();

    void poll();
    std::shared_ptr<const CameraEngineStatus> status() const { return m_status.load(); }

    static std::string paramsFilePath();
    static std::string statusFilePath();

private:
    void writeParams(const CameraEngineParams& params, bool running);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const CameraEngineStatus>> m_status;
};

} // namespace htb
