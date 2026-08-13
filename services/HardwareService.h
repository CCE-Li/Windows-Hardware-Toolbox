#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "core/config/Config.h"
#include "hardware/audio/AudioProvider.h"
#include "hardware/battery/BatteryProvider.h"
#include "hardware/camera/CameraProvider.h"
#include "hardware/camera/VideoTransformPipeline.h"
#include "hardware/camera/VirtualCameraController.h"
#include "hardware/cpu/CpuProvider.h"
#include "hardware/device/DeviceProvider.h"
#include "hardware/display/DisplayProvider.h"
#include "hardware/gpu/GpuProvider.h"
#include "hardware/memory/MemoryProvider.h"
#include "hardware/network/NetworkProvider.h"
#include "hardware/storage/StorageProvider.h"

namespace htb {

class HardwareService {
public:
    explicit HardwareService(Config config);
    ~HardwareService();

    void start();
    void stop();

    const CpuProvider& cpu() const { return *m_cpu; }
    const GpuProvider& gpu() const { return *m_gpu; }
    const MemoryProvider& memory() const { return *m_memory; }
    const DeviceProvider& device() const { return *m_device; }
    const StorageProvider& storage() const { return *m_storage; }
    const NetworkProvider& network() const { return *m_network; }
    const BatteryProvider& battery() const { return *m_battery; }
    const AudioProvider& audio() const { return *m_audio; }
    const CameraProvider& camera() const { return *m_camera; }
    const DisplayProvider& display() const { return *m_display; }
    void requestDeviceRefresh() { m_device->requestRefresh(); }

    bool launchSystemTool(const std::string& tool);
    void runPingTest(const std::string& target, int count) { m_network->runPingTest(target, count); }
    void runDnsTest(const std::string& host) { m_network->runDnsTest(host); }

    bool isElevated() const;
    void relaunchAsAdmin(const std::string& page = "");

    void setDeviceEnabledAsync(const std::string& instanceId, bool enable) {
        m_device->setDeviceEnabledAsync(instanceId, enable);
    }
    void removeDeviceAsync(const std::string& instanceId) { m_device->removeDeviceAsync(instanceId); }
    void rescanDevicesAsync() { m_device->rescanDevicesAsync(); }

    void loadDisplayModesAsync(const std::string& deviceName) { m_display->loadModesAsync(deviceName); }
    void applyDisplayModeAsync(const std::string& deviceName, uint32_t width, uint32_t height,
                               uint32_t refreshHz) {
        m_display->applyModeAsync(deviceName, width, height, refreshHz);
    }
    void setVolumeAsync(float level, bool muted) { m_audio->setVolumeAsync(level, muted); }

    void createVirtualCamera(const std::string& friendlyName) {
        m_virtualCamera->create(friendlyName);
    }
    void removeVirtualCamera() { m_virtualCamera->remove(); }
    VirtualCameraController& virtualCamera() { return *m_virtualCamera; }
    const VirtualCameraController& virtualCamera() const { return *m_virtualCamera; }

    enum class VcameraAction { None, Register, Unregister };
    void setPendingVcameraAction(VcameraAction action, const std::string& name) {
        m_pendingVcamAction = action;
        m_pendingVcamName = name;
    }
    VcameraAction pendingVcameraAction() const { return m_pendingVcamAction; }
    const std::string& pendingVcameraName() const { return m_pendingVcamName; }
    void clearPendingVcameraAction() { m_pendingVcamAction = VcameraAction::None; }

    void startCameraOutput(const CameraOutputParams& params) { m_cameraOutput->start(params); }
    void updateCameraOutput(const CameraOutputParams& params) { m_cameraOutput->updateParams(params); }
    void stopCameraOutput() { m_cameraOutput->stop(); }
    std::shared_ptr<const CameraOutputStatus> cameraOutputStatus() const {
        return m_cameraOutput->status();
    }

    const Config& config() const { return m_config; }
    int intervalMs() const { return m_intervalMs; }
    std::chrono::steady_clock::time_point lastRefresh() const;

private:
    void loop();

    Config m_config;
    int m_intervalMs;
    std::atomic<bool> m_running{false};
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::jthread m_thread;

    std::unique_ptr<CpuProvider> m_cpu;
    std::unique_ptr<GpuProvider> m_gpu;
    std::unique_ptr<MemoryProvider> m_memory;
    std::unique_ptr<DeviceProvider> m_device;
    std::unique_ptr<StorageProvider> m_storage;
    std::unique_ptr<NetworkProvider> m_network;
    std::unique_ptr<BatteryProvider> m_battery;
    std::unique_ptr<AudioProvider> m_audio;
    std::unique_ptr<CameraProvider> m_camera;
    std::unique_ptr<DisplayProvider> m_display;
    std::unique_ptr<VirtualCameraController> m_virtualCamera;
    std::unique_ptr<VideoTransformPipeline> m_cameraOutput;

    VcameraAction m_pendingVcamAction = VcameraAction::None;
    std::string m_pendingVcamName;

    std::atomic<std::chrono::steady_clock::time_point> m_lastRefresh{};
};

} // namespace htb
