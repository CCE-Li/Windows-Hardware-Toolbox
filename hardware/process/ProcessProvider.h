#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "hardware/HardwareProvider.h"
#include "monitoring/Metric.h"

namespace htb {

struct ProcessInfo {
    uint32_t pid = 0;
    uint32_t ppid = 0;
    std::string name;
    uint32_t sessionId = 0;
    uint32_t threads = 0;
    uint32_t handles = 0;
    std::string priority;   // 正常 / 高 / 实时 ...
    int basePriority = 0;

    uint64_t workingSetBytes = 0;
    uint64_t privateBytes = 0;
    uint64_t commitBytes = 0;
    uint64_t readBytes = 0;   // cumulative IO
    uint64_t writeBytes = 0;

    uint64_t kernelTime100ns = 0;
    uint64_t userTime100ns = 0;
    double cpuPercent = 0.0;      // % of total system capacity (sum ≈ system usage)
    double readRateBps = 0.0;
    double writeRateBps = 0.0;
};

struct ProcessSnapshot {
    std::vector<ProcessInfo> processes;
    double totalCpuPercent = 0.0;
    uint32_t coreCount = 0;
    uint64_t totalRamBytes = 0;
    uint64_t busyRamBytes = 0;
    std::chrono::steady_clock::time_point timestamp;
    Availability availability = Availability::Available;
    std::string source = "NT API + PSAPI";
};

struct ProcessDetail {
    uint32_t pid = 0;
    std::string name;
    std::string executablePath;
    std::string commandLine;
    std::string userName;
    std::string status;   // "ok" or error description
};

struct ProcessOperationResult {
    std::string operation;
    uint32_t pid = 0;
    bool success = false;
    std::string message;
};

enum class ProcessPriority {
    Idle,
    BelowNormal,
    Normal,
    AboveNormal,
    High,
    Realtime,
};

// Pure helpers (UI + tests).
std::string processPriorityDisplayName(ProcessPriority priority);
double computeProcessCpuPercent(uint64_t procDeltaTicks, uint64_t totalDeltaTicks);

class ProcessProvider final : public HardwareProvider {
public:
    ProcessProvider();
    ~ProcessProvider() override;

    std::string_view name() const override { return "process"; }
    void refresh() override;

    std::shared_ptr<const ProcessSnapshot> snapshot() const { return m_snapshot.load(); }

    void endProcess(uint32_t pid);
    void endProcessTree(uint32_t pid);
    void suspendProcess(uint32_t pid);
    void resumeProcess(uint32_t pid);
    void setPriority(uint32_t pid, ProcessPriority priority);
    void setAffinity(uint32_t pid, uint64_t mask);
    void restartProcess(uint32_t pid);
    std::shared_ptr<const ProcessOperationResult> lastOperation() const { return m_lastOperation.load(); }

    void inspectProcess(uint32_t pid);
    std::shared_ptr<const ProcessDetail> detailSnapshot() const { return m_detail.load(); }

private:
    void runOperation(const std::string& operation, uint32_t pid,
                      const std::function<std::pair<bool, std::string>()>& task);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const ProcessSnapshot>> m_snapshot;
    std::atomic<std::shared_ptr<const ProcessOperationResult>> m_lastOperation;
    std::atomic<std::shared_ptr<const ProcessDetail>> m_detail;
    std::atomic<uint32_t> m_inspectPid{0};
};

} // namespace htb