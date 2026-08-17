#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "hardware/HardwareProvider.h"
#include "monitoring/Metric.h"

namespace htb {

struct ServiceInfo {
    std::string name;
    std::string displayName;
    std::string description;
    std::string state;      // 运行中 / 已停止 ...
    std::string startType;  // 自动 / 手动 / 禁用 ...
    std::string binaryPath;
    uint32_t pid = 0;
    bool canStop = false;
    Availability availability = Availability::Available;
    std::string source = "SCM";
};

struct ServiceOperationResult {
    std::string operation;
    std::string name;
    bool success = false;
    std::string message;
};

class SystemServiceProvider final : public HardwareProvider {
public:
    SystemServiceProvider();
    ~SystemServiceProvider() override;

    std::string_view name() const override { return "systemservice"; }
    void refresh() override;

    std::shared_ptr<const std::vector<ServiceInfo>> snapshot() const { return m_snapshot.load(); }

    void startService(const std::string& name);
    void stopService(const std::string& name);
    void restartService(const std::string& name);
    std::shared_ptr<const ServiceOperationResult> lastOperation() const { return m_lastOperation.load(); }

private:
    void runOperation(const std::string& operation, const std::string& name,
                      const std::function<std::pair<bool, std::string>()>& task);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const std::vector<ServiceInfo>>> m_snapshot;
    std::atomic<std::shared_ptr<const ServiceOperationResult>> m_lastOperation;
};

} // namespace htb