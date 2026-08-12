#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "core/config/Config.h"
#include "hardware/cpu/CpuProvider.h"
#include "hardware/gpu/GpuProvider.h"
#include "hardware/memory/MemoryProvider.h"

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

    std::atomic<std::chrono::steady_clock::time_point> m_lastRefresh{};
};

} // namespace htb
