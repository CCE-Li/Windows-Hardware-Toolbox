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
#include "hardware/camera/CameraEngineController.h"
#include "hardware/camera/CameraProvider.h"
#include "hardware/camera/VideoTransformPipeline.h"
#include "hardware/camera/VirtualCameraController.h"
#include "hardware/cpu/CpuProvider.h"
#include "hardware/device/DeviceProvider.h"
#include "hardware/display/DisplayProvider.h"
#include "hardware/gpu/GpuProvider.h"
#include "hardware/memory/MemoryProvider.h"
#include "hardware/network/NetworkProvider.h"
#include "hardware/process/ProcessProvider.h"
#include "hardware/sensors/SensorsProvider.h"
#include "hardware/storage/StorageProvider.h"
#include "hardware/startup/StartupProvider.h"
#include "hardware/systemservice/SystemServiceProvider.h"

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
    const ProcessProvider& process() const { return *m_process; }
    const SystemServiceProvider& systemService() const { return *m_systemService; }
    const StartupProvider& startup() const { return *m_startup; }
    const SensorsProvider& sensors() const { return *m_sensors; }
    void requestDeviceRefresh() { m_device->requestRefresh(); }

    bool launchSystemTool(const std::string& tool);
    void runPingTest(const std::string& target, int count) { m_network->runPingTest(target, count); }
    void runDnsTest(const std::string& host) { m_network->runDnsTest(host); }

    bool isElevated() const;
    void relaunchAsAdmin(const std::string& page = "");

    bool setAutoStart(bool enable);
    bool autoStartEnabled() const;

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

    // ---- 任务管理器: 进程 ----
    void endProcess(uint32_t pid) { m_process->endProcess(pid); }
    void endProcessTree(uint32_t pid) { m_process->endProcessTree(pid); }
    void suspendProcess(uint32_t pid) { m_process->suspendProcess(pid); }
    void resumeProcess(uint32_t pid) { m_process->resumeProcess(pid); }
    void setProcessPriority(uint32_t pid, ProcessPriority priority) { m_process->setPriority(pid, priority); }
    void setProcessAffinity(uint32_t pid, uint64_t mask) { m_process->setAffinity(pid, mask); }
    void restartProcess(uint32_t pid) { m_process->restartProcess(pid); }
    void inspectProcess(uint32_t pid) { m_process->inspectProcess(pid); }
    std::shared_ptr<const ProcessOperationResult> processLastOperation() const {
        return m_process->lastOperation();
    }
    std::shared_ptr<const ProcessDetail> processDetail() const { return m_process->detailSnapshot(); }

    // ---- 任务管理器: 服务 ----
    void startService(const std::string& name) { m_systemService->startService(name); }
    void stopService(const std::string& name) { m_systemService->stopService(name); }
    void restartService(const std::string& name) { m_systemService->restartService(name); }
    std::shared_ptr<const ServiceOperationResult> serviceLastOperation() const {
        return m_systemService->lastOperation();
    }

    // ---- 任务管理器: 启动项 ----
    void setStartupItemEnabled(const StartupItem& item, bool enable) { m_startup->setEnabled(item, enable); }
    void removeStartupItem(const StartupItem& item) { m_startup->removeItem(item); }
    void openStartupItemLocation(const StartupItem& item) { m_startup->openLocation(item); }
    std::shared_ptr<const StartupOperationResult> startupLastOperation() const {
        return m_startup->lastOperation();
    }

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

    void startPythonEngine(const CameraEngineParams& params) { m_cameraEngine->start(params); }
    void updatePythonEngine(const CameraEngineParams& params) { m_cameraEngine->updateParams(params); }
    void stopPythonEngine() { m_cameraEngine->stop(); }
    void pollPythonEngine() { m_cameraEngine->poll(); }
    std::shared_ptr<const CameraEngineStatus> pythonEngineStatus() const {
        return m_cameraEngine->status();
    }

    const Config& config() const { return m_config; }
    void saveCameraConfig(const CameraConfig& camera) {
        m_config.camera = camera;
        m_config.save();
    }
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
    std::unique_ptr<CameraEngineController> m_cameraEngine;
    std::unique_ptr<ProcessProvider> m_process;
    std::unique_ptr<SystemServiceProvider> m_systemService;
    std::unique_ptr<StartupProvider> m_startup;
    std::unique_ptr<SensorsProvider> m_sensors;

    VcameraAction m_pendingVcamAction = VcameraAction::None;
    std::string m_pendingVcamName;

    std::atomic<std::chrono::steady_clock::time_point> m_lastRefresh{};
};

} // namespace htb
