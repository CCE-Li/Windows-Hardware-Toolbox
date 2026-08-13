#include "core/logging/Logger.h"
#include "core/runtime/Privileges.h"
#include "core/util/Utf.h"
#include "services/HardwareService.h"

#include <combaseapi.h>
#include <exception>
#include <functional>
#include <shellapi.h>
#include <windows.h>

#include <cstdlib>

namespace htb {

namespace {
void refreshProvider(const char* module, const std::function<void()>& refresh) {
    try {
        refresh();
    } catch (const std::exception& e) {
        HTB_ERROR("[service] {} refresh failed: {}", module, e.what());
    } catch (...) {
        HTB_ERROR("[service] {} refresh failed: unknown exception", module);
    }
}
} // namespace

HardwareService::HardwareService(Config config)
    : m_config(std::move(config)),
      m_intervalMs(m_config.monitoring.interval_ms),
      m_cpu(std::make_unique<CpuProvider>()),
      m_gpu(std::make_unique<GpuProvider>()),
      m_memory(std::make_unique<MemoryProvider>()),
      m_device(std::make_unique<DeviceProvider>()),
      m_storage(std::make_unique<StorageProvider>()),
      m_network(std::make_unique<NetworkProvider>()),
      m_battery(std::make_unique<BatteryProvider>()),
      m_audio(std::make_unique<AudioProvider>()),
      m_camera(std::make_unique<CameraProvider>()),
      m_display(std::make_unique<DisplayProvider>()),
      m_virtualCamera(std::make_unique<VirtualCameraController>()),
      m_cameraOutput(std::make_unique<VideoTransformPipeline>()) {}

HardwareService::~HardwareService() {
    stop();
}

void HardwareService::start() {
    if (m_running.exchange(true)) return;
    m_thread = std::jthread([this] { loop(); });
}

void HardwareService::stop() {
    if (!m_running.exchange(false)) return;
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

void HardwareService::loop() {
    HRESULT comInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(comInit)) HTB_WARN("[service] CoInitializeEx failed: {:#x}", static_cast<unsigned>(comInit));
    HTB_INFO("[service] worker started (interval {} ms)", m_intervalMs);

    std::unique_lock lock(m_mutex);
    while (m_running.load()) {
        refreshProvider("cpu", [this] { m_cpu->refresh(); });
        refreshProvider("gpu", [this] { m_gpu->refresh(); });
        refreshProvider("memory", [this] { m_memory->refresh(); });
        refreshProvider("device", [this] { m_device->refresh(); });
        refreshProvider("storage", [this] { m_storage->refresh(); });
        refreshProvider("network", [this] { m_network->refresh(); });
        refreshProvider("battery", [this] { m_battery->refresh(); });
        refreshProvider("audio", [this] { m_audio->refresh(); });
        refreshProvider("camera", [this] { m_camera->refresh(); });
        refreshProvider("display", [this] { m_display->refresh(); });
        m_lastRefresh.store(std::chrono::steady_clock::now());
        m_cv.wait_for(lock, std::chrono::milliseconds(m_intervalMs),
                      [this] { return !m_running.load(); });
    }

    HTB_INFO("[service] worker stopped");
    if (SUCCEEDED(comInit)) CoUninitialize();
}

std::chrono::steady_clock::time_point HardwareService::lastRefresh() const {
    return m_lastRefresh.load();
}

bool HardwareService::isElevated() const {
    return htb::isElevated();
}

void HardwareService::relaunchAsAdmin(const std::string& page) {
    wchar_t exePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) {
        HTB_ERROR("[service] GetModuleFileNameW failed; cannot relaunch");
        return;
    }
    std::wstring args = L"--elevated";
    if (!page.empty()) {
        args += L" --page=";
        args += toWide(page);
    }
    const HINSTANCE r = ShellExecuteW(nullptr, L"runas", exePath, args.c_str(), nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(r) <= 32) {
        HTB_ERROR("[service] UAC relaunch failed: {:#x}", static_cast<unsigned>(reinterpret_cast<INT_PTR>(r)));
    } else {
        HTB_INFO("[service] relaunching elevated (page={})", page);
    }
}

bool HardwareService::launchSystemTool(const std::string& tool) {
    const wchar_t* command = nullptr;
    if (tool == "devmgmt") {
        command = L"devmgmt.msc";
    } else if (tool == "diskmgmt") {
        command = L"diskmgmt.msc";
    } else if (tool == "msinfo32") {
        command = L"msinfo32";
    } else if (tool == "ncpa") {
        command = L"ncpa.cpl";
    } else if (tool == "taskmgr") {
        command = L"taskmgr";
    } else if (tool == "logdir") {
        wchar_t* base = nullptr;
        size_t len = 0;
        _wdupenv_s(&base, &len, L"LOCALAPPDATA");
        const std::wstring dir = (base && *base)
                                     ? (std::wstring(base) + L"\\HardwareToolbox\\logs")
                                     : L".";
        free(base);
        const HINSTANCE r = ShellExecuteW(nullptr, L"open", L"explorer.exe", dir.c_str(), nullptr, SW_SHOWNORMAL);
        return reinterpret_cast<INT_PTR>(r) > 32;
    } else if (tool == "cfgdir") {
        wchar_t* base = nullptr;
        size_t len = 0;
        _wdupenv_s(&base, &len, L"LOCALAPPDATA");
        const std::wstring dir = (base && *base)
                                     ? (std::wstring(base) + L"\\HardwareToolbox")
                                     : L".";
        free(base);
        const HINSTANCE r = ShellExecuteW(nullptr, L"open", L"explorer.exe", dir.c_str(), nullptr, SW_SHOWNORMAL);
        return reinterpret_cast<INT_PTR>(r) > 32;
    } else {
        HTB_WARN("[service] unknown system tool: {}", tool);
        return false;
    }

    const HINSTANCE r = ShellExecuteW(nullptr, L"open", command, nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(r) <= 32) {
        HTB_ERROR("[service] failed to launch {}: {:#x}", tool, static_cast<unsigned>(reinterpret_cast<INT_PTR>(r)));
        return false;
    }
    HTB_INFO("[service] launched system tool {}", tool);
    return true;
}

} // namespace htb
