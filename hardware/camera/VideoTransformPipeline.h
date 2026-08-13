#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace htb {

struct CameraOutputParams {
    int cameraIndex = 0;
    int outputTarget = 0;  // 0 = OBS Virtual Camera, 1 = Hardware Toolbox Virtual Camera
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    bool flipHorizontal = false;
    bool flipVertical = false;
    int rotation = 0;
    int brightness = 0;
    int contrast = 0;
    int saturation = 0;
};

struct CameraOutputStatus {
    bool running = false;
    bool capturing = false;
    std::string message;
    uint32_t sourceWidth = 0;
    uint32_t sourceHeight = 0;
    uint64_t framesSent = 0;
    double fps = 0.0;
};

class VideoTransformPipeline {
public:
    VideoTransformPipeline();
    ~VideoTransformPipeline();

    void start(const CameraOutputParams& params);
    void updateParams(const CameraOutputParams& params);
    void stop();

    std::shared_ptr<const CameraOutputStatus> status() const { return m_status.load(); }

private:
    void loop();
    void writeSharedMemory(const uint8_t* frame, size_t size);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const CameraOutputStatus>> m_status;
    std::atomic<CameraOutputParams> m_params;
};

} // namespace htb
