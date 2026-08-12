#include "core/logging/Logger.h"
#include "services/HardwareService.h"

#include <combaseapi.h>
#include <exception>
#include <functional>
#include <windows.h>

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
      m_network(std::make_unique<NetworkProvider>()) {}

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

} // namespace htb
